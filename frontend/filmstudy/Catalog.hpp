#pragma once

#include <QSqlDatabase>
#include <QString>

#include <memory>

class QLockFile;

namespace FilmStudy {

// Owns one writable SQLite connection and its catalog lock. The catalog cannot
// be used if the schema is newer or an older migration has not been implemented.
class Catalog final {
public:
	Catalog() = default;
	~Catalog();
	Catalog(const Catalog &) = delete;
	Catalog &operator=(const Catalog &) = delete;

	bool Open(const QString &rootDirectory, QString *error);
	void Close();
	bool IsOpen() const { return database.isOpen(); }
	QString CatalogId() const;

private:
	bool Initialize(QString *error);
	bool CheckExisting(QString *error);
	bool Exec(const QString &sql, QString *error);
	bool CapabilityCheck(QString *error);

	QString connectionName;
	QSqlDatabase database;
	std::unique_ptr<QLockFile> lock;
};

} // namespace FilmStudy
