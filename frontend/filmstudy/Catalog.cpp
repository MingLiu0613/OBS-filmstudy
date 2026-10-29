#include "Catalog.hpp"
#include "SchemaV2.generated.hpp"
#include "third_party/sqlite/sqlite3.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QUuid>

#include <cstddef>
#include <limits>

using namespace FilmStudy;

namespace {
QString UtcNow()
{
	return QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH:mm:ss.zzzZ");
}

bool SetError(QString *error, const QString &message)
{
	if (error)
		*error = message;
	return false;
}

QString DbError(sqlite3 *database)
{
	return QString::fromUtf8(database ? sqlite3_errmsg(database) : "SQLite could not allocate a connection");
}
} // namespace

Catalog::~Catalog()
{
	Close();
}

bool Catalog::Open(const QString &rootDirectory, QString *error)
{
	Close();
	const QFileInfo rootInfo(rootDirectory);
	if (!rootInfo.exists() && !QDir().mkpath(rootDirectory))
		return SetError(error, QStringLiteral("Cannot create catalog directory"));
	const QString root = QDir(rootDirectory).absolutePath();
	this->rootDirectory = root;
	lock = std::make_unique<QLockFile>(QDir(root).filePath("catalog.lock"));
	lock->setStaleLockTime(0);
	if (!lock->tryLock(0)) {
		lock.reset();
		return SetError(error, QStringLiteral("Catalog is already open for writing"));
	}

	const QString dbPath = QDir(root).filePath("catalog.sqlite3");
	const QFileInfo dbInfo(dbPath);
	const bool existing = dbInfo.exists() && dbInfo.size() > 0;
	const QByteArray pathBytes = dbPath.toUtf8();
	if (sqlite3_open_v2(pathBytes.constData(), &database,
			    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK) {
		const QString message = DbError(database);
		Close();
		return SetError(error, message);
	}
	if (!Exec(QStringLiteral("PRAGMA foreign_keys=ON"), error) ||
	    !Exec(QStringLiteral("PRAGMA busy_timeout=5000"), error) ||
	    !Exec(QStringLiteral("PRAGMA journal_mode=WAL"), error) ||
	    !Exec(QStringLiteral("PRAGMA synchronous=FULL"), error) || !CapabilityCheck(error) ||
	    !(existing ? CheckExisting(error) : Initialize(error))) {
		Close();
		return false;
	}
	for (const char *directory : {"media", "pending", "backups"}) {
		if (!QDir().mkpath(QDir(root).filePath(QString::fromUtf8(directory)))) {
			Close();
			return SetError(error, QStringLiteral("Cannot create catalog subdirectory"));
		}
	}
	return true;
}

void Catalog::Close()
{
	if (database) {
		sqlite3_close(database);
		database = nullptr;
	}
	lock.reset();
	rootDirectory.clear();
}

bool Catalog::Exec(const QString &sql, QString *error)
{
	const QByteArray bytes = sql.toUtf8();
	char *message = nullptr;
	const int result = sqlite3_exec(database, bytes.constData(), nullptr, nullptr, &message);
	if (result == SQLITE_OK)
		return true;
	const QString detail = message ? QString::fromUtf8(message) : DbError(database);
	sqlite3_free(message);
	return SetError(error, detail);
}

bool Catalog::ExecuteParams(const char *sql, const QStringList &values, QString *error)
{
	sqlite3_stmt *statement = nullptr;
	if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK)
		return SetError(error, DbError(database));
	if (values.size() > std::numeric_limits<int>::max() ||
	    sqlite3_bind_parameter_count(statement) != static_cast<int>(values.size())) {
		sqlite3_finalize(statement);
		return SetError(error, QStringLiteral("SQL parameter count mismatch"));
	}
	for (int index = 0; index < values.size(); ++index) {
		const QByteArray bytes = values[index].toUtf8();
		if (bytes.size() > std::numeric_limits<int>::max() ||
		    sqlite3_bind_text(statement, index + 1, bytes.constData(), static_cast<int>(bytes.size()), SQLITE_TRANSIENT) !=
		    SQLITE_OK) {
			const QString detail = DbError(database);
			sqlite3_finalize(statement);
			return SetError(error, detail);
		}
	}
	const int result = sqlite3_step(statement);
	const QString detail = result == SQLITE_DONE ? QString() : DbError(database);
	sqlite3_finalize(statement);
	return detail.isEmpty() ? true : SetError(error, detail);
}

bool Catalog::QueryValue(const char *sql, QString *value, bool *hasRow, QString *error)
{
	sqlite3_stmt *statement = nullptr;
	if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK)
		return SetError(error, DbError(database));
	const int result = sqlite3_step(statement);
	*hasRow = result == SQLITE_ROW;
	if (*hasRow && value) {
		const auto *bytes = sqlite3_column_text(statement, 0);
		*value = bytes ? QString::fromUtf8(reinterpret_cast<const char *>(bytes)) : QString();
	}
	const QString detail = result == SQLITE_ROW || result == SQLITE_DONE ? QString() : DbError(database);
	sqlite3_finalize(statement);
	return detail.isEmpty() ? true : SetError(error, detail);
}

bool Catalog::QueryBoundValue(const char *sql, const QString &parameter, QString *value, bool *hasRow,
			      QString *error)
{
	sqlite3_stmt *statement = nullptr;
	if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK)
		return SetError(error, DbError(database));
	const QByteArray bytes = parameter.toUtf8();
	if (bytes.size() > std::numeric_limits<int>::max() ||
	    sqlite3_bind_text(statement, 1, bytes.constData(), static_cast<int>(bytes.size()), SQLITE_TRANSIENT) !=
		SQLITE_OK) {
		const QString detail = DbError(database);
		sqlite3_finalize(statement);
		return SetError(error, detail);
	}
	const int result = sqlite3_step(statement);
	*hasRow = result == SQLITE_ROW;
	if (*hasRow && value) {
		const auto *data = sqlite3_column_text(statement, 0);
		*value = data ? QString::fromUtf8(reinterpret_cast<const char *>(data)) : QString();
	}
	const QString detail = result == SQLITE_ROW || result == SQLITE_DONE ? QString() : DbError(database);
	sqlite3_finalize(statement);
	return detail.isEmpty() ? true : SetError(error, detail);
}

bool Catalog::CapabilityCheck(QString *error)
{
	if (sqlite3_libversion_number() < 3038000)
		return SetError(error, QStringLiteral("SQLite 3.38+ is required"));
	QString value;
	bool row = false;
	if (!QueryValue("SELECT json_valid('{}')", &value, &row, error) || !row || value != QStringLiteral("1"))
		return SetError(error, QStringLiteral("SQLite JSON support is required"));
	return Exec(QStringLiteral("CREATE TEMP TABLE filmstudy_strict_check (value INTEGER) STRICT"), error) &&
	       Exec(QStringLiteral("DROP TABLE filmstudy_strict_check"), error);
}

bool Catalog::Initialize(QString *error)
{
	if (!Exec(QStringLiteral("BEGIN IMMEDIATE"), error))
		return false;
	bool ok = true;
	// The first embedded statement sets foreign_keys, already set before BEGIN.
	for (std::size_t index = 1; index < filmStudySchemaV2.size(); ++index) {
		const auto statement = filmStudySchemaV2[index];
		if (!Exec(QString::fromUtf8(statement.data(), qsizetype(statement.size())), error)) {
			ok = false;
			break;
		}
	}
	if (ok) {
		const QString now = UtcNow();
		const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();
		ok = Exec(QStringLiteral("INSERT INTO catalog_info(singleton,catalog_id,created_at,updated_at) "
				 "VALUES (1,'%1','%2','%2')")
				  .arg(id, now),
			  error);
	}
	if (ok) {
		const QString hash = QString::fromUtf8(filmStudySchemaV2Sha256.data(),
						       qsizetype(filmStudySchemaV2Sha256.size()));
		ok = Exec(QStringLiteral("INSERT INTO schema_migrations(version,migration_name,script_sha256,"
				 "applied_at,application_version) VALUES (2,'0002_initial','%1','%2','filmstudy-dev')")
				  .arg(hash, UtcNow()),
			  error);
	}
	if (ok) {
		bool row = false;
		ok = QueryValue("PRAGMA foreign_key_check", nullptr, &row, error) && !row;
		if (!ok)
			SetError(error, QStringLiteral("Schema foreign key check failed"));
	}
	if (!ok) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	if (!Exec(QStringLiteral("COMMIT"), error)) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	return true;
}

bool Catalog::CheckExisting(QString *error)
{
	QString value;
	bool row = false;
	if (!QueryValue("PRAGMA user_version", &value, &row, error) || !row)
		return SetError(error, QStringLiteral("Cannot read catalog schema version"));
	if (value != QStringLiteral("2"))
		return SetError(error, QStringLiteral("Unsupported catalog schema version: %1").arg(value));
	if (!QueryValue("SELECT script_sha256 FROM schema_migrations WHERE version=2", &value, &row, error) ||
	    !row || value != QString::fromUtf8(filmStudySchemaV2Sha256.data(),
						    qsizetype(filmStudySchemaV2Sha256.size())))
		return SetError(error, QStringLiteral("Catalog migration history does not match schema"));
	if (!QueryValue("PRAGMA foreign_key_check", nullptr, &row, error) || row)
		return SetError(error, QStringLiteral("Catalog foreign key check failed"));
	if (!QueryValue("PRAGMA integrity_check", &value, &row, error) || !row || value != QStringLiteral("ok"))
		return SetError(error, QStringLiteral("Catalog integrity check failed"));
	return true;
}

QString Catalog::CatalogId() const
{
	QString value;
	bool row = false;
	if (const_cast<Catalog *>(this)->QueryValue("SELECT catalog_id FROM catalog_info WHERE singleton=1", &value,
						     &row, nullptr) && row)
		return value;
	return {};
}

bool Catalog::CreateGame(const QString &name, QString *gameId, QString *error)
{
	if (!database || name.trimmed().isEmpty())
		return SetError(error, QStringLiteral("Open a catalog and provide a game name"));
	const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();
	const QString now = UtcNow();
	if (!ExecuteParams("INSERT INTO games(id,name,created_at,updated_at) VALUES (?,?,?,?)",
			   {id, name.trimmed(), now, now}, error))
		return false;
	if (gameId)
		*gameId = id;
	return true;
}

bool Catalog::BeginManualCapture(const QString &gameId, const QString &sourceSnapshotJson,
				 ManualCaptureIds *ids, QString *error)
{
	if (!database || !ids)
		return SetError(error, QStringLiteral("Open a catalog before capture"));
	const QString sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();
	const QString clipId = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();
	const QString operationId = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();
	const QString now = UtcNow();
	const QString finalPath = QStringLiteral("media/%1.mp4").arg(clipId);
	if (!Exec(QStringLiteral("BEGIN IMMEDIATE"), error))
		return false;
	const bool ok = ExecuteParams(
			"INSERT INTO capture_sessions(id,game_id,mode,state,source_snapshot_json,created_at,updated_at) "
			"VALUES (?,?,'manual','starting',?,?,?)",
			{sessionId, gameId, sourceSnapshotJson, now, now}, error) &&
		ExecuteParams("INSERT INTO clips(id,game_id,capture_mode,capture_state,created_at,updated_at) "
			      "VALUES (?,?,'manual','starting',?,?)",
			      {clipId, gameId, now, now}, error) &&
		ExecuteParams("INSERT INTO capture_operations(id,session_id,clip_id,phase,intended_final_relpath,"
			      "created_at,updated_at) VALUES (?,?,?,'starting',?,?,?)",
			      {operationId, sessionId, clipId, finalPath, now, now}, error) &&
		ExecuteParams("INSERT INTO operation_events(id,session_id,operation_id,dedupe_key,event_type,"
			      "created_at) VALUES (?,?,?,'start-requested','start_requested',?)",
			      {QUuid::createUuid().toString(QUuid::WithoutBraces).toLower(), sessionId, operationId, now},
			      error);
	if (!ok) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	if (!Exec(QStringLiteral("COMMIT"), error)) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	*ids = {clipId, sessionId, operationId};
	return true;
}

bool Catalog::MarkRecording(const QString &operationId, QString *error)
{
	if (!database)
		return SetError(error, QStringLiteral("Catalog is not open"));
	if (!Exec(QStringLiteral("BEGIN IMMEDIATE"), error))
		return false;
	const QString now = UtcNow();
	bool ok = ExecuteParams("UPDATE capture_operations SET phase='recording',updated_at=? "
				"WHERE id=? AND phase='starting'",
				{now, operationId}, error);
	if (ok && sqlite3_changes(database) != 1)
		ok = SetError(error, QStringLiteral("Operation is not starting"));
	if (ok)
		ok = ExecuteParams("UPDATE capture_sessions SET state='active',started_at=?,updated_at=? "
				   "WHERE id=(SELECT session_id FROM capture_operations WHERE id=?)",
				   {now, now, operationId}, error) &&
		     ExecuteParams("UPDATE clips SET capture_state='recording',updated_at=? "
				   "WHERE id=(SELECT clip_id FROM capture_operations WHERE id=?)",
				   {now, operationId}, error);
	if (ok)
		ok = ExecuteParams("INSERT INTO operation_events(id,session_id,operation_id,dedupe_key,"
				   "event_type,created_at) SELECT ?,session_id,id,'recording-started',"
				   "'recording_started',? FROM capture_operations WHERE id=?",
				   {QUuid::createUuid().toString(QUuid::WithoutBraces).toLower(), now, operationId},
				   error);
	if (!ok) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	if (!Exec(QStringLiteral("COMMIT"), error)) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	return true;
}

bool Catalog::RequestStop(const QString &operationId, QString *error)
{
	if (!database)
		return SetError(error, QStringLiteral("Catalog is not open"));
	QString phase;
	bool row = false;
	if (!QueryBoundValue("SELECT phase FROM capture_operations WHERE id=?", operationId, &phase, &row,
			     error) || !row)
		return SetError(error, QStringLiteral("Unknown capture operation"));
	if (phase == QStringLiteral("saving") || phase == QStringLiteral("processing"))
		return true;
	if (phase != QStringLiteral("recording"))
		return SetError(error, QStringLiteral("Operation is not recording"));
	if (!Exec(QStringLiteral("BEGIN IMMEDIATE"), error))
		return false;
	const QString now = UtcNow();
	bool ok = ExecuteParams("UPDATE capture_operations SET phase='saving',updated_at=? "
				"WHERE id=? AND phase='recording'",
				{now, operationId}, error);
	if (ok && sqlite3_changes(database) != 1)
		ok = SetError(error, QStringLiteral("Operation state changed during stop"));
	if (ok)
		ok = ExecuteParams("UPDATE capture_sessions SET state='stopping',updated_at=? "
				   "WHERE id=(SELECT session_id FROM capture_operations WHERE id=?)",
				   {now, operationId}, error) &&
		     ExecuteParams("UPDATE clips SET capture_state='saving',updated_at=? "
				   "WHERE id=(SELECT clip_id FROM capture_operations WHERE id=?)",
				   {now, operationId}, error);
	if (!ok) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	if (!Exec(QStringLiteral("COMMIT"), error)) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	return true;
}

bool Catalog::MarkFailed(const QString &operationId, const QString &code, const QString &message,
			 QString *error)
{
	if (!database || code.isEmpty())
		return SetError(error, QStringLiteral("Open a catalog and provide a failure code"));
	if (!Exec(QStringLiteral("BEGIN IMMEDIATE"), error))
		return false;
	const QString now = UtcNow();
	bool ok = ExecuteParams("UPDATE capture_operations SET phase='failed',error_code=?,error_message=?,"
				"updated_at=? WHERE id=? AND phase IN ('created','starting','recording','saving',"
				"'processing')",
				{code, message, now, operationId}, error);
	if (ok && sqlite3_changes(database) != 1)
		ok = SetError(error, QStringLiteral("Operation cannot become failed"));
	if (ok)
		ok = ExecuteParams("UPDATE capture_sessions SET state='failed',stopped_at=?,updated_at=? "
				   "WHERE id=(SELECT session_id FROM capture_operations WHERE id=?)",
				   {now, now, operationId}, error) &&
		     ExecuteParams("UPDATE clips SET capture_state='failed',updated_at=? "
				   "WHERE id=(SELECT clip_id FROM capture_operations WHERE id=?)",
				   {now, operationId}, error);
	if (ok)
		ok = ExecuteParams("INSERT INTO operation_events(id,session_id,operation_id,dedupe_key,"
				   "event_type,payload_json,created_at) SELECT ?,session_id,id,'capture-failed',"
				   "'capture_failed',json_object('code',?),? FROM capture_operations WHERE id=?",
				   {QUuid::createUuid().toString(QUuid::WithoutBraces).toLower(), code, now,
				    operationId},
				   error);
	if (!ok) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	if (!Exec(QStringLiteral("COMMIT"), error)) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	return true;
}

bool Catalog::MarkRecoveryRequired(const QString &operationId, const QString &code,
				   const QString &message, QString *error)
{
	if (!database || code.isEmpty())
		return SetError(error, QStringLiteral("Open a catalog and provide a recovery code"));
	if (!Exec(QStringLiteral("BEGIN IMMEDIATE"), error))
		return false;
	const QString now = UtcNow();
	bool ok = ExecuteParams("UPDATE capture_operations SET phase='recovery_required',error_code=?,"
				"error_message=?,updated_at=? WHERE id=? AND phase='processing'",
				{code, message, now, operationId}, error);
	if (ok && sqlite3_changes(database) != 1)
		ok = SetError(error, QStringLiteral("Operation is not awaiting media publication"));
	if (ok)
		ok = ExecuteParams("UPDATE capture_sessions SET state='recovery_required',updated_at=? "
				   "WHERE id=(SELECT session_id FROM capture_operations WHERE id=?)",
				   {now, operationId}, error) &&
		     ExecuteParams("UPDATE clips SET capture_state='recovery_required',updated_at=? "
				   "WHERE id=(SELECT clip_id FROM capture_operations WHERE id=?)",
				   {now, operationId}, error) &&
		     ExecuteParams("INSERT INTO operation_events(id,session_id,operation_id,dedupe_key,"
				   "event_type,payload_json,created_at) SELECT ?,session_id,id,'recovery-required',"
				   "'recovery_required',json_object('code',?),? FROM capture_operations WHERE id=?",
				   {QUuid::createUuid().toString(QUuid::WithoutBraces).toLower(), code, now,
				    operationId}, error);
	if (!ok) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	if (!Exec(QStringLiteral("COMMIT"), error)) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	return true;
}

bool Catalog::MarkOutputClosed(const QString &operationId, const QString &absoluteOutputPath, QString *error)
{
	if (!database || !QFileInfo(absoluteOutputPath).isAbsolute())
		return SetError(error, QStringLiteral("An explicit absolute OBS output path is required"));
	QString phase;
	bool row = false;
	if (!QueryBoundValue("SELECT phase FROM capture_operations WHERE id=?", operationId, &phase, &row,
			     error) || !row)
		return SetError(error, QStringLiteral("Unknown capture operation"));
	if (phase == QStringLiteral("processing") || phase == QStringLiteral("ready")) {
		QString recordedPath;
		bool found = false;
		if (!QueryBoundValue("SELECT json_extract(payload_json,'$.output_path') FROM operation_events "
				     "WHERE operation_id=? AND dedupe_key='output-closed'", operationId,
				     &recordedPath, &found, error))
			return false;
		return found && recordedPath == absoluteOutputPath
			       ? true
			       : SetError(error, QStringLiteral("Closed output path differs from recorded operation"));
	}
	if (phase != QStringLiteral("saving"))
		return SetError(error, QStringLiteral("Operation is not saving"));
	if (!Exec(QStringLiteral("BEGIN IMMEDIATE"), error))
		return false;
	const QString now = UtcNow();
	const QString payload = QString::fromUtf8(
		QJsonDocument(QJsonObject{{QStringLiteral("output_path"), absoluteOutputPath}})
			.toJson(QJsonDocument::Compact));
	bool ok = ExecuteParams("UPDATE capture_operations SET phase='processing',updated_at=? "
				"WHERE id=? AND phase='saving'",
				{now, operationId}, error);
	if (ok && sqlite3_changes(database) != 1)
		ok = SetError(error, QStringLiteral("Operation state changed before output closed"));
	if (ok)
		ok = ExecuteParams("UPDATE capture_sessions SET state='stopped',stopped_at=?,updated_at=? "
				   "WHERE id=(SELECT session_id FROM capture_operations WHERE id=?)",
				   {now, now, operationId}, error) &&
		     ExecuteParams("UPDATE clips SET capture_state='processing',updated_at=? "
				   "WHERE id=(SELECT clip_id FROM capture_operations WHERE id=?)",
				   {now, operationId}, error) &&
		     ExecuteParams("INSERT INTO operation_events(id,session_id,operation_id,dedupe_key,"
				   "event_type,payload_json,created_at) SELECT ?,session_id,id,'output-closed',"
				   "'output_closed',?,? FROM capture_operations WHERE id=?",
				   {QUuid::createUuid().toString(QUuid::WithoutBraces).toLower(), payload, now,
				    operationId},
				   error);
	if (!ok) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	if (!Exec(QStringLiteral("COMMIT"), error)) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	return true;
}

QString Catalog::IntendedPath(const QString &operationId) const
{
	if (!database)
		return {};
	QString value;
	bool row = false;
	if (const_cast<Catalog *>(this)->QueryBoundValue(
		    "SELECT intended_final_relpath FROM capture_operations WHERE id=?", operationId, &value,
		    &row, nullptr) &&
	    row)
		return value;
	return {};
}

bool Catalog::PublishOriginal(const QString &operationId, const VerifiedMedia &media, QString *error)
{
	if (!database || media.clipId.isEmpty() || media.relativePath != IntendedPath(operationId) ||
	    media.sizeBytes < 1 || media.durationMs < 1 || media.width < 1 || media.height < 1 ||
	    media.fpsNum < 1 || media.fpsDen < 1 || media.sha256.size() != 64)
		return SetError(error, QStringLiteral("Verified media does not match the capture operation"));
	QString clipId;
	QString phase;
	bool row = false;
	if (!QueryBoundValue("SELECT clip_id FROM capture_operations WHERE id=?", operationId, &clipId, &row,
			     error) ||
	    !row || clipId != media.clipId ||
	    !QueryBoundValue("SELECT phase FROM capture_operations WHERE id=?", operationId, &phase, &row,
			     error) ||
	    !row || phase != QStringLiteral("processing"))
		return SetError(error, QStringLiteral("Operation is not processing this clip"));
	const QFileInfo file(QDir(rootDirectory).filePath(media.relativePath));
	if (!file.isFile() || file.size() != media.sizeBytes)
		return SetError(error, QStringLiteral("Verified media file is unavailable or changed"));
	if (!Exec(QStringLiteral("BEGIN IMMEDIATE"), error))
		return false;
	const QString now = UtcNow();
	const QString assetId = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();
	const QString locationId = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();
	bool ok = ExecuteParams(
		"INSERT INTO media_assets(id,clip_id,asset_kind,container,video_codec,audio_codec,width,height,"
		"fps_num,fps_den,duration_ms,size_bytes,sha256,created_at) "
		"VALUES (?1,?2,'original','mp4',?3,NULLIF(?4,''),?5,?6,?7,?8,?9,?10,?11,?12)",
		{assetId, clipId, media.videoCodec, media.audioCodec, QString::number(media.width),
		 QString::number(media.height), QString::number(media.fpsNum), QString::number(media.fpsDen),
		 QString::number(media.durationMs), QString::number(media.sizeBytes), media.sha256, now},
		error);
	if (ok)
		ok = ExecuteParams(
			"INSERT INTO media_locations(id,asset_id,relative_path,path_key,availability,"
			"is_preferred,last_verified_at,observed_size_bytes,created_at,updated_at) "
			"VALUES (?1,?2,?3,?4,'available',1,?5,?6,?7,?8)",
			{locationId, assetId, media.relativePath, media.relativePath, now,
			 QString::number(media.sizeBytes), now, now},
			error);
	if (ok)
		ok = ExecuteParams("UPDATE clips SET primary_asset_id=?,capture_state='ready',"
				   "revision=revision+1,updated_at=? WHERE id=? AND capture_state='processing'",
				   {assetId, now, clipId}, error);
	if (ok && sqlite3_changes(database) != 1)
		ok = SetError(error, QStringLiteral("Clip changed during media publication"));
	if (ok)
		ok = ExecuteParams("UPDATE capture_operations SET final_asset_id=?,actual_output_relpath=?,"
				   "phase='ready',updated_at=? WHERE id=? AND phase='processing'",
				   {assetId, media.relativePath, now, operationId}, error) &&
		     ExecuteParams("UPDATE catalog_info SET content_revision=content_revision+1,updated_at=? "
				   "WHERE singleton=1",
				   {now}, error) &&
		     ExecuteParams("INSERT INTO operation_events(id,session_id,operation_id,dedupe_key,"
				   "event_type,created_at) SELECT ?,session_id,id,'media-published',"
				   "'media_published',? FROM capture_operations WHERE id=?",
				   {QUuid::createUuid().toString(QUuid::WithoutBraces).toLower(), now, operationId},
				   error);
	if (!ok) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	if (!Exec(QStringLiteral("COMMIT"), error)) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	return true;
}

bool Catalog::CreateTopic(const QString &name, QString *topicId, QString *error)
{
	const QString label = name.trimmed().normalized(QString::NormalizationForm_C);
	if (!database || label.isEmpty() || !topicId)
		return SetError(error, QStringLiteral("Open a catalog and provide a topic name"));
	QString existingId;
	bool existing = false;
	if (!QueryBoundValue("SELECT id FROM topics WHERE name_key=? AND is_active=1",
			     label.toCaseFolded(), &existingId, &existing, error))
		return false;
	if (existing) {
		*topicId = existingId;
		return true;
	}
	const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();
	const QString now = UtcNow();
	if (!Exec(QStringLiteral("BEGIN IMMEDIATE"), error))
		return false;
	const bool ok = ExecuteParams("INSERT INTO topics(id,name,name_key,created_at,updated_at) "
				      "VALUES (?,?,?,?,?)",
				      {id, label, label.toCaseFolded(), now, now}, error) &&
			ExecuteParams("UPDATE catalog_info SET content_revision=content_revision+1,updated_at=? "
				      "WHERE singleton=1", {now}, error);
	if (!ok) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	if (!Exec(QStringLiteral("COMMIT"), error)) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	*topicId = id;
	return true;
}

bool Catalog::FindTopic(const QString &name, QString *topicId, QString *error)
{
	if (!database || !topicId)
		return SetError(error, QStringLiteral("Open a catalog before looking up a topic"));
	bool found = false;
	topicId->clear();
	return QueryBoundValue("SELECT id FROM topics WHERE name_key=? AND is_active=1",
			       name.trimmed().normalized(QString::NormalizationForm_C).toCaseFolded(),
			       topicId, &found, error);
}

bool Catalog::CreateDraftAnalysis(const QString &clipId, QString *analysisId, QString *error)
{
	if (!database || clipId.isEmpty() || !analysisId)
		return SetError(error, QStringLiteral("Open a catalog and select a ready clip"));
	const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();
	const QString now = UtcNow();
	if (!Exec(QStringLiteral("BEGIN IMMEDIATE"), error))
		return false;
	bool ok = ExecuteParams("INSERT INTO analyses(id,clip_id,reference_asset_id,created_at,updated_at) "
				"SELECT ?,id,primary_asset_id,?,? FROM clips "
				"WHERE id=? AND capture_state='ready' AND deleted_at IS NULL",
				{id, now, now, clipId}, error);
	if (ok && sqlite3_changes(database) != 1)
		ok = SetError(error, QStringLiteral("Clip is not ready for analysis"));
	if (ok)
		ok = ExecuteParams("UPDATE catalog_info SET content_revision=content_revision+1,updated_at=? "
				   "WHERE singleton=1", {now}, error);
	if (!ok) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	if (!Exec(QStringLiteral("COMMIT"), error)) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	*analysisId = id;
	return true;
}

bool Catalog::CompleteAnalysis(const QString &analysisId, const QString &perspective,
			       const QString &topicId, const QString &assessment,
			       const QString &note, QString *error)
{
	if (!database || analysisId.isEmpty() || topicId.isEmpty() ||
	    (perspective != QStringLiteral("offense") && perspective != QStringLiteral("defense")) ||
	    (assessment != QStringLiteral("positive") && assessment != QStringLiteral("negative")))
		return SetError(error, QStringLiteral("Complete analysis requires perspective, topic and assessment"));
	if (!Exec(QStringLiteral("BEGIN IMMEDIATE"), error))
		return false;
	const QString now = UtcNow();
	bool ok = ExecuteParams("UPDATE analyses SET perspective=?,topic_id=?,assessment=?,note=?,"
				"status='complete',revision=revision+1,updated_at=? "
				"WHERE id=? AND deleted_at IS NULL AND clip_id IN "
				"(SELECT id FROM clips WHERE capture_state='ready' AND deleted_at IS NULL)",
				{perspective, topicId, assessment, note, now, analysisId}, error);
	if (ok && sqlite3_changes(database) != 1)
		ok = SetError(error, QStringLiteral("Analysis is unavailable or its clip is not ready"));
	if (ok)
		ok = ExecuteParams("UPDATE catalog_info SET content_revision=content_revision+1,updated_at=? "
				   "WHERE singleton=1", {now}, error);
	if (!ok) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	if (!Exec(QStringLiteral("COMMIT"), error)) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}
	return true;
}

bool Catalog::SearchComplete(const QString &perspective, const QString &topicId,
			     const QString &assessment, QList<ClipMatch> *matches, QString *error)
{
	if (!database || !matches)
		return SetError(error, QStringLiteral("Open a catalog before searching"));
	const char *sql = "SELECT v.clip_id,a.id,v.relative_path FROM v_clip_primary_media v "
			  "JOIN analyses a ON a.clip_id=v.clip_id AND a.reference_asset_id=v.asset_id "
			  "WHERE v.capture_state='ready' AND v.availability='available' "
			  "AND a.deleted_at IS NULL AND a.status='complete' "
			  "AND (?1 IS NULL OR a.perspective=?1) "
			  "AND (?2 IS NULL OR a.topic_id=?2) "
			  "AND (?3 IS NULL OR a.assessment=?3) "
			  "ORDER BY v.created_at DESC,v.clip_id,a.created_at,a.id";
	sqlite3_stmt *statement = nullptr;
	if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK)
		return SetError(error, DbError(database));
	const QStringList filters{perspective, topicId, assessment};
	for (int index = 0; index < filters.size(); ++index) {
		const QByteArray bytes = filters[index].toUtf8();
		if (bytes.size() > std::numeric_limits<int>::max()) {
			sqlite3_finalize(statement);
			return SetError(error, QStringLiteral("Search filter is too large"));
		}
		const int result = filters[index].isEmpty()
				   ? sqlite3_bind_null(statement, index + 1)
				   : sqlite3_bind_text(statement, index + 1, bytes.constData(),
						       static_cast<int>(bytes.size()),
						       SQLITE_TRANSIENT);
		if (result != SQLITE_OK) {
			const QString detail = DbError(database);
			sqlite3_finalize(statement);
			return SetError(error, detail);
		}
	}
	QList<ClipMatch> found;
	int step = SQLITE_ROW;
	while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
		const auto column = [statement](int index) {
			const auto *bytes = sqlite3_column_text(statement, index);
			return bytes ? QString::fromUtf8(reinterpret_cast<const char *>(bytes)) : QString();
		};
		const QString clipId = column(0);
		if (found.isEmpty() || found.last().clipId != clipId)
			found.append({clipId, column(2), {}});
		found.last().matchedAnalysisIds.append(column(1));
	}
	const QString detail = step == SQLITE_DONE ? QString() : DbError(database);
	sqlite3_finalize(statement);
	if (!detail.isEmpty())
		return SetError(error, detail);
	*matches = found;
	return true;
}
