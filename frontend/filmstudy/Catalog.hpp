#pragma once

#include <QString>
#include <QStringList>
#include <QLockFile>
#include <QList>

#include <memory>

struct sqlite3;

namespace FilmStudy {

struct ManualCaptureIds {
	QString clipId;
	QString sessionId;
	QString operationId;
};

struct VerifiedMedia {
	QString clipId;
	QString relativePath;
	QString videoCodec;
	QString audioCodec;
	QString sha256;
	int width = 0;
	int height = 0;
	int fpsNum = 0;
	int fpsDen = 0;
	qint64 durationMs = 0;
	qint64 sizeBytes = 0;
};

struct ClipMatch {
	QString clipId;
	QString relativePath;
	QStringList matchedAnalysisIds;
};

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
	bool CreateGame(const QString &name, QString *gameId, QString *error);
	bool BeginManualCapture(const QString &gameId, const QString &sourceSnapshotJson,
				ManualCaptureIds *ids, QString *error);
	bool MarkRecording(const QString &operationId, QString *error);
	bool RequestStop(const QString &operationId, QString *error);
	bool MarkFailed(const QString &operationId, const QString &code, const QString &message,
			QString *error);
	bool MarkRecoveryRequired(const QString &operationId, const QString &code,
				  const QString &message, QString *error);
	bool MarkOutputClosed(const QString &operationId, const QString &absoluteOutputPath, QString *error);
	bool PublishOriginal(const QString &operationId, const VerifiedMedia &media, QString *error);
	bool CreateTopic(const QString &name, QString *topicId, QString *error);
	bool FindTopic(const QString &name, QString *topicId, QString *error);
	bool CreateDraftAnalysis(const QString &clipId, QString *analysisId, QString *error);
	bool CompleteAnalysis(const QString &analysisId, const QString &perspective,
			      const QString &topicId, const QString &assessment,
			      const QString &note, int expectedRevision, QString *error);
	bool SearchComplete(const QString &perspective, const QString &topicId,
			    const QString &assessment, QList<ClipMatch> *matches, QString *error);
	// Freezes the current same-analysis query in SQLite before copying media.
	// A failed copy keeps the frozen job for inspection and recovery.
	bool ExportMatching(const QString &perspective, const QString &topicId,
			    const QString &assessment, const QString &destinationRoot,
			    QString *packagePath, QString *error);
	QString IntendedPath(const QString &operationId) const;
	QString RootDirectory() const { return rootDirectory; }

private:
	bool Initialize(QString *error);
	bool CheckExisting(QString *error);
	bool Exec(const QString &sql, QString *error);
	bool ExecuteParams(const char *sql, const QStringList &values, QString *error);
	bool QueryValue(const char *sql, QString *value, bool *hasRow, QString *error);
	bool QueryBoundValue(const char *sql, const QString &parameter, QString *value, bool *hasRow,
			     QString *error);
	bool CapabilityCheck(QString *error);

	sqlite3 *database = nullptr;
	QString rootDirectory;
	std::unique_ptr<QLockFile> lock;
};

} // namespace FilmStudy
