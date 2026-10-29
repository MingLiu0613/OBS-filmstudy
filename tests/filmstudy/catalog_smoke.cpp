#include "Catalog.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
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
	if (!primary.MarkRecoveryRequired(capture.operationId, QStringLiteral("import_failed"),
					  QStringLiteral("Completed file needs retry"), &error))
		return 10;
	FilmStudy::ManualCaptureIds next;
	if (!primary.BeginManualCapture(gameId, QStringLiteral("{}"), &next, &error) ||
	    next.clipId == capture.clipId || next.sessionId == capture.sessionId)
		return 11;
	if (!primary.MarkRecording(next.operationId, &error) ||
	    !primary.RequestStop(next.operationId, &error) ||
	    !primary.MarkOutputClosed(next.operationId, output, &error))
		return 13;
	// This synthetic file tests catalog publication and query semantics only.
	// MediaImporter separately probes real MP4 before it calls PublishOriginal.
	const QString relative = primary.IntendedPath(next.operationId);
	QFile media(QDir(temporary.path()).filePath(relative));
	const QByteArray bytes("catalog smoke fixture");
	if (!media.open(QIODevice::WriteOnly) || media.write(bytes) != bytes.size())
		return 14;
	media.close();
	FilmStudy::VerifiedMedia verified;
	verified.clipId = next.clipId;
	verified.relativePath = relative;
	verified.videoCodec = QStringLiteral("h264");
	verified.sha256 = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
	verified.width = 1920;
	verified.height = 1080;
	verified.fpsNum = 60;
	verified.fpsDen = 1;
	verified.durationMs = 1000;
	verified.sizeBytes = bytes.size();
	if (!primary.PublishOriginal(next.operationId, verified, &error)) {
		std::cerr << "catalog publication failed: " << error.toStdString() << '\n';
		return 15;
	}
	QString offenseTopic;
	QString defenseTopic;
	QString offenseAnalysis;
	QString defenseAnalysis;
	if (!primary.CreateTopic(QStringLiteral("Contact timing"), &offenseTopic, &error) ||
	    !primary.CreateTopic(QStringLiteral("Transition defense"), &defenseTopic, &error) ||
	    !primary.CreateDraftAnalysis(next.clipId, &offenseAnalysis, &error) ||
	    !primary.CreateDraftAnalysis(next.clipId, &defenseAnalysis, &error) ||
	    !primary.CompleteAnalysis(offenseAnalysis, QStringLiteral("offense"), offenseTopic,
				      QStringLiteral("positive"), QStringLiteral("Good contact"), 1, &error) ||
	    !primary.CompleteAnalysis(defenseAnalysis, QStringLiteral("defense"), defenseTopic,
				      QStringLiteral("negative"), QStringLiteral("Late rotation"), 1, &error)) {
		std::cerr << "analysis creation failed: " << error.toStdString() << '\n';
		return 16;
	}
	QList<FilmStudy::ClipMatch> matches;
	if (!primary.SearchComplete(QStringLiteral("offense"), defenseTopic, {}, &matches, &error) ||
	    !matches.isEmpty())
		return 17;
	if (!primary.SearchComplete(QStringLiteral("offense"), offenseTopic, {}, &matches, &error) ||
	    matches.size() != 1 || matches.first().clipId != next.clipId ||
	    matches.first().matchedAnalysisIds != QStringList{offenseAnalysis})
		return 18;
	if (primary.CompleteAnalysis(offenseAnalysis, QStringLiteral("defense"), defenseTopic,
				     QStringLiteral("negative"), QStringLiteral("Stale write"), 1, &error))
		return 19;
	QTemporaryDir exports;
	if (!exports.isValid())
		return 20;
	QString package;
	if (!primary.ExportMatching(QStringLiteral("offense"), offenseTopic, {},
				    exports.path(), &package, &error)) {
		std::cerr << "export failed: " << error.toStdString() << '\n';
		return 21;
	}
	QFile exportedMedia(QDir(package).filePath(QStringLiteral("media/%1.mp4").arg(next.clipId)));
	if (!exportedMedia.open(QIODevice::ReadOnly) || exportedMedia.readAll() != bytes)
		return 22;
	QFile manifestFile(QDir(package).filePath("manifest.json"));
	if (!manifestFile.open(QIODevice::ReadOnly))
		return 23;
	const QByteArray frozenBytes = manifestFile.readAll();
	const QJsonObject manifest = QJsonDocument::fromJson(frozenBytes).object();
	const QJsonArray exportedClips = manifest.value("clips").toArray();
	if (manifest.value("schema_version").toInt() != 2 || manifest.value("clip_count").toInt() != 1 ||
	    manifest.value("analysis_count").toInt() != 2 || exportedClips.size() != 1 ||
	    exportedClips.first().toObject().value("matched_analysis_ids").toArray().first().toString() !=
		    offenseAnalysis ||
	    exportedClips.first().toObject().value("analyses").toArray().size() != 2)
		return 24;
	if (!primary.CompleteAnalysis(offenseAnalysis, QStringLiteral("offense"), offenseTopic,
				      QStringLiteral("negative"), QStringLiteral("Revised after export"), 2, &error))
		return 26;
	manifestFile.seek(0);
	if (manifestFile.readAll() != frozenBytes)
		return 27;
	QString noPackage;
	if (primary.ExportMatching(QStringLiteral("offense"), defenseTopic, {},
				   exports.path(), &noPackage, &error) || !noPackage.isEmpty())
		return 25;
	primary.Close();
	if (!concurrent.Open(temporary.path(), &error) || concurrent.CatalogId() != id) {
		std::cerr << "reopen failed: " << error.toStdString() << '\n';
		return 5;
	}
	std::cout << "catalog create, lock, publication, same-analysis query and frozen export passed\n";
	return 0;
}
