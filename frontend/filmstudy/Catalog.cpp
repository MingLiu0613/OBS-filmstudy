#include "Catalog.hpp"
#include "SchemaV2.generated.hpp"
#include "third_party/sqlite/sqlite3.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QUuid>

#include <cstddef>

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
	return QString::fromUtf8(sqlite3_errmsg(database));
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
