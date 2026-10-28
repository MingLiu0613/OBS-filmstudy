#include "Catalog.hpp"
#include "SchemaV2.generated.hpp"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QSqlError>
#include <QSqlQuery>
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
	connectionName = QStringLiteral("filmstudy-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
	database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
	database.setDatabaseName(dbPath);
	if (!database.open()) {
		const QString message = database.lastError().text();
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
	if (database.isValid()) {
		const QString name = connectionName;
		database.close();
		database = QSqlDatabase();
		QSqlDatabase::removeDatabase(name);
	}
	connectionName.clear();
	lock.reset();
}

bool Catalog::Exec(const QString &sql, QString *error)
{
	QSqlQuery query(database);
	if (!query.exec(sql))
		return SetError(error, query.lastError().text());
	return true;
}

bool Catalog::CapabilityCheck(QString *error)
{
	QSqlQuery query(database);
	if (!query.exec(QStringLiteral("SELECT sqlite_version(), json_valid('{}')")) || !query.next())
		return SetError(error, QStringLiteral("SQLite JSON support is required"));
	const QStringList version = query.value(0).toString().split('.');
	if (version.size() < 2 || version[0].toInt() < 3 ||
	    (version[0].toInt() == 3 && version[1].toInt() < 38) || query.value(1).toInt() != 1)
		return SetError(error, QStringLiteral("SQLite 3.38+ with JSON support is required"));
	query.finish();
	if (!Exec(QStringLiteral("CREATE TEMP TABLE filmstudy_strict_check (value INTEGER) STRICT"), error))
		return false;
	return Exec(QStringLiteral("DROP TABLE filmstudy_strict_check"), error);
}

bool Catalog::Initialize(QString *error)
{
	if (!Exec(QStringLiteral("BEGIN IMMEDIATE"), error))
		return false;
	bool ok = true;
	for (std::size_t index = 1; index < filmStudySchemaV2.size(); ++index) {
		const auto statement = filmStudySchemaV2[index];
		if (!Exec(QString::fromUtf8(statement.data(), qsizetype(statement.size())), error)) {
			ok = false;
			break;
		}
	}
	if (ok) {
		QSqlQuery query(database);
		query.prepare(QStringLiteral("INSERT INTO catalog_info(singleton,catalog_id,created_at,updated_at) "
					     "VALUES (1,?,?,?)"));
		query.addBindValue(QUuid::createUuid().toString(QUuid::WithoutBraces).toLower());
		query.addBindValue(UtcNow());
		query.addBindValue(UtcNow());
		ok = query.exec();
		if (!ok)
			SetError(error, query.lastError().text());
	}
	if (ok) {
		QSqlQuery query(database);
		query.prepare(QStringLiteral("INSERT INTO schema_migrations(version,migration_name,script_sha256,"
					     "applied_at,application_version) VALUES (2,?,?,?,?)"));
		query.addBindValue(QStringLiteral("0002_initial"));
		query.addBindValue(QString::fromUtf8(filmStudySchemaV2Sha256.data(),
								 qsizetype(filmStudySchemaV2Sha256.size())));
		query.addBindValue(UtcNow());
		query.addBindValue(QStringLiteral("filmstudy-dev"));
		ok = query.exec();
		if (!ok)
			SetError(error, query.lastError().text());
	}
	if (ok) {
		QSqlQuery query(database);
		ok = query.exec(QStringLiteral("PRAGMA foreign_key_check")) && !query.next();
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
	QSqlQuery query(database);
	if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next())
		return SetError(error, QStringLiteral("Cannot read catalog schema version"));
	const int version = query.value(0).toInt();
	if (version != 2)
		return SetError(error, QStringLiteral("Unsupported catalog schema version: %1").arg(version));
	if (!query.exec(QStringLiteral("SELECT script_sha256 FROM schema_migrations WHERE version=2")) ||
	    !query.next() ||
	    query.value(0).toString() != QString::fromUtf8(filmStudySchemaV2Sha256.data(),
								   qsizetype(filmStudySchemaV2Sha256.size())))
		return SetError(error, QStringLiteral("Catalog migration history does not match schema"));
	if (!query.exec(QStringLiteral("PRAGMA foreign_key_check")) || query.next())
		return SetError(error, QStringLiteral("Catalog foreign key check failed"));
	if (!query.exec(QStringLiteral("PRAGMA integrity_check")) || !query.next() ||
	    query.value(0).toString() != QStringLiteral("ok"))
		return SetError(error, QStringLiteral("Catalog integrity check failed"));
	return true;
}

QString Catalog::CatalogId() const
{
	QSqlQuery query(database);
	if (query.exec(QStringLiteral("SELECT catalog_id FROM catalog_info WHERE singleton=1")) && query.next())
		return query.value(0).toString();
	return {};
}
