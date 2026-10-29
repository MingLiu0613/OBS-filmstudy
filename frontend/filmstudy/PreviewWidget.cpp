#include "PreviewWidget.hpp"

#include <QPaintEvent>
#include <QResizeEvent>

#include <algorithm>

PreviewWidget::PreviewWidget(QWidget *parent) : QWidget(parent)
{
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);
	setMinimumSize(480, 270);
}

PreviewWidget::~PreviewWidget()
{
	if (display) {
		obs_display_remove_draw_callback(display, Draw, this);
		obs_display_destroy(display);
	}
}

void PreviewWidget::SetSource(obs_source_t *newSource)
{
	std::lock_guard<std::mutex> guard(sourceMutex);
	source = newSource;
}

void PreviewWidget::EnsureDisplay()
{
	if (display || !isVisible())
		return;
	gs_init_data info = {};
	info.cx = std::max(1, int(width() * devicePixelRatioF()));
	info.cy = std::max(1, int(height() * devicePixelRatioF()));
	info.format = GS_BGRA;
	info.zsformat = GS_ZS_NONE;
#ifdef _WIN32
	info.window.hwnd = reinterpret_cast<void *>(winId());
#elif defined(__APPLE__)
	info.window.view = reinterpret_cast<id>(winId());
#else
	return;
#endif
	display = obs_display_create(&info, 0xff000000);
	if (display)
		obs_display_add_draw_callback(display, Draw, this);
}

void PreviewWidget::paintEvent(QPaintEvent *event)
{
	EnsureDisplay();
	QWidget::paintEvent(event);
}

void PreviewWidget::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	if (display)
		obs_display_resize(display, std::max(1, int(width() * devicePixelRatioF())),
				   std::max(1, int(height() * devicePixelRatioF())));
}

void PreviewWidget::Draw(void *data, uint32_t width, uint32_t height)
{
	auto *widget = static_cast<PreviewWidget *>(data);
	std::lock_guard<std::mutex> guard(widget->sourceMutex);
	if (!widget->source)
		return;
	const uint32_t sourceWidth = std::max(1u, obs_source_get_width(widget->source));
	const uint32_t sourceHeight = std::max(1u, obs_source_get_height(widget->source));
	const float scale = std::min(float(width) / float(sourceWidth), float(height) / float(sourceHeight));
	const int drawWidth = std::max(1, int(float(sourceWidth) * scale));
	const int drawHeight = std::max(1, int(float(sourceHeight) * scale));
	const int x = (int(width) - drawWidth) / 2;
	const int y = (int(height) - drawHeight) / 2;
	gs_viewport_push();
	gs_projection_push();
	gs_ortho(0.0f, float(sourceWidth), 0.0f, float(sourceHeight), -100.0f, 100.0f);
	gs_set_viewport(x, y, drawWidth, drawHeight);
	obs_source_video_render(widget->source);
	gs_projection_pop();
	gs_viewport_pop();
}
