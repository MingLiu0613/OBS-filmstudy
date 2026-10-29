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
	QString gameId;
	if (!primary.CreateGame(QStringLiteral("Example game"), &gameId, &error)) {
		std::cerr << "game creation failed: " << error.toStdString() << '\n';
		return 6;
	}
	FilmStudy::ManualCaptureIds capture;
	if (!primary.BeginManualCapture(gameId, QStringLiteral("{}"), &capture, &error) ||
	    capture.clipId.isEmpty() || capture.sessionId.isEmpty() || capture.operationId.isEmpty()) {
		std::cerr << "capture creation failed: " << error.toStdString() << '\n';
		return 7;
	}
	FilmStudy::ManualCaptureIds overlapping;
	if (primary.BeginManualCapture(gameId, QStringLiteral("{}"), &overlapping, &error))
		return 8;
	if (!primary.MarkRecording(capture.operationId, &error) || !primary.RequestStop(capture.operationId, &error) ||
	    !primary.RequestStop(capture.operationId, &error)) {
		std::cerr << "recording state transition failed: " << error.toStdString() << '\n';
		return 9;
	}
	const QString output = QDir(temporary.path()).filePath("completed.mp4");
	if (!primary.MarkOutputClosed(capture.operationId, output, &error) ||
	    !primary.MarkOutputClosed(capture.operationId, output, &error) ||
	    primary.IntendedPath(capture.operationId) != QStringLiteral("media/%1.mp4").arg(capture.clipId)) {
		std::cerr << "completed output transition failed: " << error.toStdString() << '\n';
		return 12;
	}
	if (!primary.MarkFailed(capture.operationId, QStringLiteral("output_failed"),
				QStringLiteral("No completed file"), &error))
		return 10;
	FilmStudy::ManualCaptureIds next;
	if (!primary.BeginManualCapture(gameId, QStringLiteral("{}"), &next, &error) ||
	    next.clipId == capture.clipId || next.sessionId == capture.sessionId)
		return 11;
	primary.Close();
	if (!concurrent.Open(temporary.path(), &error) || concurrent.CatalogId() != id) {
		std::cerr << "reopen failed: " << error.toStdString() << '\n';
		return 5;
	}
	std::cout << "catalog schema 2 create, lock and reopen passed\n";
	return 0;
}
