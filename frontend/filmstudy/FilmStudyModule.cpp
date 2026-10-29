#include "Catalog.hpp"
#include "MediaImporter.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/bmem.h>
#include <util/config-file.h>

#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>

OBS_DECLARE_MODULE()

namespace {
QString TakeObsString(char *value)
{
	const QString result = value ? QString::fromUtf8(value) : QString();
	bfree(value);
	return result;
}

class FilmStudyController final : public QObject {
public:
	FilmStudyController() : catalog(std::make_unique<FilmStudy::Catalog>()) {}
	~FilmStudyController() override
	{
		if (importThread) {
			importThread->wait();
			delete importThread;
		}
		delete dialog.data();
	}

	void Show()
	{
		if (!active.operationId.isEmpty())
			return;
		if (!dialog) {
			dialog = new QDialog(static_cast<QWidget *>(obs_frontend_get_main_window()));
			dialog->setWindowTitle(QStringLiteral("Basketball Film Study"));
			dialog->setMinimumWidth(380);
			auto *layout = new QVBoxLayout(dialog);
			status = new QLabel(QStringLiteral("Select a catalog and game before recording."), dialog);
			status->setWordWrap(true);
			layout->addWidget(status);
			auto *choose = new QPushButton(QStringLiteral("Choose catalog and game"), dialog);
			layout->addWidget(choose);
			start = new QPushButton(QStringLiteral("Start Film Study recording"), dialog);
			layout->addWidget(start);
			addAnalysis = new QPushButton(QStringLiteral("Add analysis to last saved clip"), dialog);
			layout->addWidget(addAnalysis);
			search = new QPushButton(QStringLiteral("Search analysed clips"), dialog);
			layout->addWidget(search);
			connect(choose, &QPushButton::clicked, this, [this] { Configure(); });
			connect(start, &QPushButton::clicked, this, [this] { Start(); });
			connect(addAnalysis, &QPushButton::clicked, this, [this] { AddAnalysis(); });
			connect(search, &QPushButton::clicked, this, [this] { Search(); });
		}
		start->setEnabled(catalog->IsOpen() && active.operationId.isEmpty() && !importThread);
		addAnalysis->setEnabled(!lastReadyClipId.isEmpty() && active.operationId.isEmpty());
		search->setEnabled(catalog->IsOpen() && active.operationId.isEmpty());
		dialog->show();
		dialog->raise();
		dialog->activateWindow();
	}

	void OnEvent(obs_frontend_event event)
	{
		if (active.operationId.isEmpty())
			return;
		QString error;
		if (event == OBS_FRONTEND_EVENT_RECORDING_STARTED) {
			if (!catalog->MarkRecording(active.operationId, &error))
				SetStatus(QStringLiteral("Recording state failed: %1").arg(error));
			return;
		}
		if (event == OBS_FRONTEND_EVENT_RECORDING_STOPPING) {
			if (!catalog->RequestStop(active.operationId, &error))
				SetStatus(QStringLiteral("Saving state failed: %1").arg(error));
			return;
		}
		if (event == OBS_FRONTEND_EVENT_RECORDING_STOPPED)
			Complete();
	}

private:
	void SetStatus(const QString &message)
	{
		if (status)
			status->setText(message);
		blog(LOG_INFO, "Film Study: %s", message.toUtf8().constData());
	}

	void Configure()
	{
		if (!active.operationId.isEmpty() || importThread)
			return;
		const QString directory = QFileDialog::getExistingDirectory(
			dialog.data(), QStringLiteral("Choose Film Study catalog directory"));
		if (directory.isEmpty())
			return;
		bool accepted = false;
		const QString game = QInputDialog::getText(dialog.data(), QStringLiteral("Game"),
			QStringLiteral("Game name"), QLineEdit::Normal, {}, &accepted).trimmed();
		if (!accepted || game.isEmpty())
			return;
		QString error;
		gameId.clear();
		start->setEnabled(false);
		lastReadyClipId.clear();
		analysisCount = 0;
		if (addAnalysis)
			addAnalysis->setEnabled(false);
		if (search)
			search->setEnabled(false);
		if (!catalog->Open(directory, &error) || !catalog->CreateGame(game, &gameId, &error)) {
			QMessageBox::warning(dialog.data(), QStringLiteral("Film Study catalog"), error);
			return;
		}
		SetStatus(QStringLiteral("Ready: %1 — %2").arg(directory, game));
		start->setEnabled(true);
		search->setEnabled(true);
	}

	bool RecordingPreflight(QString *error) const
	{
		if (!catalog->IsOpen() || gameId.isEmpty() || obs_frontend_recording_active()) {
			*error = QStringLiteral("Choose a catalog and stop any active OBS recording first.");
			return false;
		}
		config_t *config = obs_frontend_get_profile_config();
		if (!config) {
			*error = QStringLiteral("OBS profile configuration is unavailable.");
			return false;
		}
		const QString mode = QString::fromUtf8(config_get_string(config, "Output", "Mode"));
		if (mode == QStringLiteral("Advanced") &&
		    QString::fromUtf8(config_get_string(config, "AdvOut", "RecType")) != QStringLiteral("Standard")) {
			*error = QStringLiteral("Set Advanced Recording Type to Standard for MP4 capture.");
			return false;
		}
		const QString format = QString::fromUtf8(config_get_string(
			config, mode == QStringLiteral("Advanced") ? "AdvOut" : "SimpleOutput", "RecFormat2"));
		if (format != QStringLiteral("mp4")) {
			*error = QStringLiteral("Set OBS Recording Format to MP4 before Film Study capture.");
			return false;
		}
		obs_source_t *scene = obs_frontend_get_current_scene();
		if (!scene) {
			*error = QStringLiteral("Select an OBS scene with a capture source first.");
			return false;
		}
		obs_source_release(scene);
		return true;
	}

	void Start()
	{
		QString error;
		if (!RecordingPreflight(&error)) {
			QMessageBox::warning(dialog.data(), QStringLiteral("Film Study recording"), error);
			return;
		}
		obs_source_t *scene = obs_frontend_get_current_scene();
		const QString sceneName = QString::fromUtf8(obs_source_get_name(scene));
		obs_source_release(scene);
		const QString snapshot = QString::fromUtf8(QJsonDocument(
			QJsonObject{{QStringLiteral("scene"), sceneName}}).toJson(QJsonDocument::Compact));
		if (!catalog->BeginManualCapture(gameId, snapshot, &active, &error)) {
			QMessageBox::warning(dialog.data(), QStringLiteral("Film Study recording"), error);
			return;
		}
		previousOutput = TakeObsString(obs_frontend_get_last_recording());
		start->setEnabled(false);
		if (addAnalysis)
			addAnalysis->setEnabled(false);
		if (search)
			search->setEnabled(false);
		SetStatus(QStringLiteral("Recording clip %1").arg(active.clipId));
		dialog->hide();
		const QString operationId = active.operationId;
		obs_frontend_recording_start();
		QTimer::singleShot(30000, this, [this, operationId] {
			if (active.operationId == operationId && !importThread &&
			    !obs_frontend_recording_active())
				Fail(QStringLiteral("start_failed"),
				     QStringLiteral("OBS did not start the requested recording."));
		});
	}

	void Complete()
	{
		QString error;
		if (!catalog->RequestStop(active.operationId, &error)) {
			Fail(QStringLiteral("state_error"), error);
			return;
		}
		const QString output = TakeObsString(obs_frontend_get_last_recording());
		if (output.isEmpty() || output == previousOutput ||
		    QFileInfo(output).suffix().compare(QStringLiteral("mp4"), Qt::CaseInsensitive) != 0) {
			Fail(QStringLiteral("output_missing"), QStringLiteral("OBS did not finish a new MP4 output."));
			return;
		}
		if (!catalog->MarkOutputClosed(active.operationId, output, &error)) {
			Fail(QStringLiteral("catalog_error"), error);
			return;
		}
		SetStatus(QStringLiteral("Verifying completed MP4 for clip %1").arg(active.clipId));
		const FilmStudy::ManualCaptureIds capture = active;
		const QString root = catalog->RootDirectory();
		const QString relative = catalog->IntendedPath(capture.operationId);
		QPointer<FilmStudyController> receiver(this);
		importThread = QThread::create([receiver, capture, root, relative, output] {
			FilmStudy::VerifiedMedia media;
			QString error;
			const bool ok = FilmStudy::MediaImporter::Import(output, root, capture.clipId,
								      relative, &media, &error);
			if (receiver)
				QMetaObject::invokeMethod(receiver.data(), [receiver, capture, media, error, ok] {
					if (receiver)
						receiver->ImportFinished(capture, media, error, ok);
				}, Qt::QueuedConnection);
		});
		importThread->start();
	}

	void ImportFinished(const FilmStudy::ManualCaptureIds &capture,
			    const FilmStudy::VerifiedMedia &media, const QString &importError, bool imported)
	{
		if (importThread) {
			importThread->wait();
			delete importThread;
			importThread = nullptr;
		}
		if (capture.operationId != active.operationId)
			return;
		QString error;
		if (!imported || !catalog->PublishOriginal(capture.operationId, media, &error)) {
			const QString reason = imported ? error : importError;
			QString recoveryError;
			if (!catalog->MarkRecoveryRequired(capture.operationId,
					QStringLiteral("media_import_failed"), reason, &recoveryError))
				SetStatus(QStringLiteral("Capture recovery required: %1; %2")
						  .arg(reason, recoveryError));
			else
				SetStatus(QStringLiteral("Capture recovery required: %1").arg(reason));
			active = {};
			if (start)
				start->setEnabled(true);
			return;
		}
		SetStatus(QStringLiteral("Saved clip %1. Open Film Study to continue.").arg(capture.clipId));
		lastReadyClipId = capture.clipId;
		analysisCount = 0;
		active = {};
		if (start)
			start->setEnabled(true);
		if (addAnalysis)
			addAnalysis->setEnabled(true);
		if (search)
			search->setEnabled(true);
	}

	void Search()
	{
		if (!catalog->IsOpen() || !active.operationId.isEmpty())
			return;
		bool accepted = false;
		const QString perspective = QInputDialog::getItem(
			dialog.data(), QStringLiteral("Search perspective"), QStringLiteral("Perspective"),
			{QStringLiteral("any"), QStringLiteral("offense"), QStringLiteral("defense")},
			0, false, &accepted);
		if (!accepted)
			return;
		const QString topicName = QInputDialog::getText(
			dialog.data(), QStringLiteral("Search topic"),
			QStringLiteral("Topic name (blank for any)"), QLineEdit::Normal, {}, &accepted).trimmed();
		if (!accepted)
			return;
		QString topicId;
		QString error;
		if (!topicName.isEmpty() && !catalog->FindTopic(topicName, &topicId, &error)) {
			QMessageBox::warning(dialog.data(), QStringLiteral("Search failed"), error);
			return;
		}
		QList<FilmStudy::ClipMatch> matches;
		if ((topicName.isEmpty() || !topicId.isEmpty()) &&
		    !catalog->SearchComplete(perspective == QStringLiteral("any") ? QString() : perspective,
					     topicId, {}, &matches, &error)) {
			QMessageBox::warning(dialog.data(), QStringLiteral("Search failed"), error);
			return;
		}
		QStringList lines;
		int analysisHits = 0;
		for (const auto &match : matches) {
			analysisHits += match.matchedAnalysisIds.size();
			if (lines.size() < 20)
				lines.append(QStringLiteral("%1 — analyses: %2")
						     .arg(match.clipId, match.matchedAnalysisIds.join(", ")));
		}
		if (matches.size() > lines.size())
			lines.append(QStringLiteral("Showing the first 20 clips."));
		SetStatus(QStringLiteral("Search: %1 clips, %2 matching analyses")
				  .arg(matches.size()).arg(analysisHits));
		QMessageBox::information(dialog.data(), QStringLiteral("Film Study search"),
			lines.isEmpty() ? QStringLiteral("No matching clips.") : lines.join("\n"));
	}

	void AddAnalysis()
	{
		if (lastReadyClipId.isEmpty() || !active.operationId.isEmpty())
			return;
		bool accepted = false;
		const QString perspective = QInputDialog::getItem(
			dialog.data(), QStringLiteral("Analysis perspective"), QStringLiteral("Perspective"),
			{QStringLiteral("offense"), QStringLiteral("defense")}, 0, false, &accepted);
		if (!accepted)
			return;
		const QString topicName = QInputDialog::getText(
			dialog.data(), QStringLiteral("Analysis topic"), QStringLiteral("Topic name"),
			QLineEdit::Normal, {}, &accepted).trimmed();
		if (!accepted || topicName.isEmpty())
			return;
		const QString assessment = QInputDialog::getItem(
			dialog.data(), QStringLiteral("Analysis assessment"), QStringLiteral("Assessment"),
			{QStringLiteral("positive"), QStringLiteral("negative")}, 0, false, &accepted);
		if (!accepted)
			return;
		const QString note = QInputDialog::getMultiLineText(
			dialog.data(), QStringLiteral("Analysis note"), QStringLiteral("Note"), {}, &accepted);
		if (!accepted)
			return;
		QString topicId;
		QString analysisId;
		QString error;
		if (!catalog->CreateTopic(topicName, &topicId, &error) ||
		    !catalog->CreateDraftAnalysis(lastReadyClipId, &analysisId, &error) ||
		    !catalog->CompleteAnalysis(analysisId, perspective, topicId, assessment, note, &error)) {
			QMessageBox::warning(dialog.data(), QStringLiteral("Analysis was not saved"), error);
			return;
		}
		++analysisCount;
		SetStatus(QStringLiteral("Clip %1: %2 analyses saved. Latest ID: %3")
				  .arg(lastReadyClipId).arg(analysisCount).arg(analysisId));
	}

	void Fail(const QString &code, const QString &message)
	{
		QString stateError;
		if (!catalog->MarkFailed(active.operationId, code, message, &stateError))
			SetStatus(QStringLiteral("Capture recovery required: %1; %2").arg(message, stateError));
		else
			SetStatus(QStringLiteral("Capture failed: %1").arg(message));
		active = {};
		if (start)
			start->setEnabled(true);
	}

	std::unique_ptr<FilmStudy::Catalog> catalog;
	QPointer<QDialog> dialog;
	QPointer<QLabel> status;
	QPointer<QPushButton> start;
	QPointer<QPushButton> addAnalysis;
	QPointer<QPushButton> search;
	QThread *importThread = nullptr;
	QString gameId;
	QString previousOutput;
	QString lastReadyClipId;
	int analysisCount = 0;
	FilmStudy::ManualCaptureIds active;
};

FilmStudyController *controller = nullptr;

void OpenFilmStudy(void *)
{
	if (controller)
		controller->Show();
}

void OnFrontendEvent(obs_frontend_event event, void *)
{
	if (controller)
		QMetaObject::invokeMethod(controller, [event] {
			if (controller)
				controller->OnEvent(event);
		}, Qt::QueuedConnection);
}
} // namespace

bool obs_module_load(void)
{
	return true;
}

void obs_module_post_load(void)
{
	controller = new FilmStudyController;
	obs_frontend_add_tools_menu_item("Basketball Film Study", OpenFilmStudy, nullptr);
	obs_frontend_add_event_callback(OnFrontendEvent, nullptr);
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(OnFrontendEvent, nullptr);
	delete controller;
	controller = nullptr;
}
