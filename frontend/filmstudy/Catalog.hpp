#pragma once

#include <QString>

#include <memory>

class QLockFile;
struct sqlite3;

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
	bool IsOpen() const { return database != nullptr; }
	QString CatalogId() const;

private:
	bool Initialize(QString *error);
	bool CheckExisting(QString *error);
	bool Exec(const QString &sql, QString *error);
	bool QueryValue(const char *sql, QString *value, bool *hasRow, QString *error);
	bool CapabilityCheck(QString *error);

	sqlite3 *database = nullptr;
	std::unique_ptr<QLockFile> lock;
};

} // namespace FilmStudy
