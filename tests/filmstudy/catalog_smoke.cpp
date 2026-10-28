#include "Catalog.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#include <iostream>

int main(int argc, char *argv[])
{
	QCoreApplication app(argc, argv);
	QTemporaryDir temporary;
	if (!temporary.isValid())
		return 1;
	FilmStudy::Catalog primary;
	FilmStudy::Catalog concurrent;
	QString error;
	if (!primary.Open(temporary.path(), &error)) {
		std::cerr << "initial open failed: " << error.toStdString() << '\n';
		return 2;
	}
	const QString id = primary.CatalogId();
	if (id.isEmpty() || !QFileInfo::exists(QDir(temporary.path()).filePath("catalog.sqlite3")) ||
	    !QFileInfo(QDir(temporary.path()).filePath("media")).isDir())
		return 3;
	if (concurrent.Open(temporary.path(), &error))
		return 4;
	primary.Close();
	if (!concurrent.Open(temporary.path(), &error) || concurrent.CatalogId() != id) {
		std::cerr << "reopen failed: " << error.toStdString() << '\n';
		return 5;
	}
	std::cout << "catalog schema 2 create, lock and reopen passed\n";
	return 0;
}
