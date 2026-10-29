#pragma once

#include <QWidget>

#include <obs.h>

#include <mutex>

class PreviewWidget final : public QWidget {
public:
	explicit PreviewWidget(QWidget *parent = nullptr);
	~PreviewWidget() override;
	void SetSource(obs_source_t *source);

protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	void EnsureDisplay();
	static void Draw(void *data, uint32_t width, uint32_t height);

	obs_display_t *display = nullptr;
	obs_source_t *source = nullptr; // Owned by the preview dialog.
	std::mutex sourceMutex;
};
