#include "MediaImporter.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
}

#include <cstring>

using namespace FilmStudy;

namespace {
bool SetError(QString *error, const QString &message)
{
	if (error)
		*error = message;
	return false;
}

bool ProbeMp4(const QString &path, VerifiedMedia *media, QString *error)
{
	AVFormatContext *context = nullptr;
	const QByteArray name = path.toUtf8();
	if (avformat_open_input(&context, name.constData(), nullptr, nullptr) < 0)
		return SetError(error, QStringLiteral("Cannot open completed video for probing"));
	const int streamInfo = avformat_find_stream_info(context, nullptr);
	if (streamInfo < 0 || !context->iformat || !std::strstr(context->iformat->name, "mp4")) {
		avformat_close_input(&context);
		return SetError(error, QStringLiteral("Completed output is not a readable MP4"));
	}
	const int videoIndex = av_find_best_stream(context, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	if (videoIndex < 0) {
		avformat_close_input(&context);
		return SetError(error, QStringLiteral("Completed MP4 has no video stream"));
	}
	AVStream *video = context->streams[videoIndex];
	AVRational fps = av_guess_frame_rate(context, video, nullptr);
	if (fps.num <= 0 || fps.den <= 0)
		fps = video->avg_frame_rate;
	qint64 durationMs = context->duration == AV_NOPTS_VALUE ? 0 : context->duration / 1000;
	if (durationMs <= 0 && video->duration != AV_NOPTS_VALUE)
		durationMs = av_rescale_q(video->duration, video->time_base, AVRational{1, 1000});
	if (video->codecpar->width <= 0 || video->codecpar->height <= 0 || fps.num <= 0 || fps.den <= 0 ||
	    durationMs <= 0) {
		avformat_close_input(&context);
		return SetError(error, QStringLiteral("Completed MP4 has invalid media properties"));
	}
	media->width = video->codecpar->width;
	media->height = video->codecpar->height;
	media->fpsNum = fps.num;
	media->fpsDen = fps.den;
	media->durationMs = durationMs;
	media->videoCodec = QString::fromUtf8(avcodec_get_name(video->codecpar->codec_id));
	const int audioIndex = av_find_best_stream(context, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
	media->audioCodec = audioIndex >= 0
				    ? QString::fromUtf8(avcodec_get_name(context->streams[audioIndex]->codecpar->codec_id))
				    : QString();
	avformat_close_input(&context);
	return true;
}

bool HashFile(const QString &path, QByteArray *digest, QString *error)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return SetError(error, QStringLiteral("Cannot reopen copied media for verification"));
	QCryptographicHash hash(QCryptographicHash::Sha256);
	while (!file.atEnd()) {
		const QByteArray block = file.read(1024 * 1024);
		if (block.isEmpty() && file.error() != QFile::NoError)
			return SetError(error, QStringLiteral("Cannot read copied media for verification"));
		hash.addData(block);
	}
	*digest = hash.result();
	return true;
}
} // namespace

bool MediaImporter::Import(const QString &obsOutputPath, const QString &catalogRoot,
			   const QString &clipId, const QString &relativePath, VerifiedMedia *result,
			   QString *error)
{
	if (!result || clipId.isEmpty() || relativePath != QStringLiteral("media/%1.mp4").arg(clipId) ||
	    !QFileInfo(obsOutputPath).isAbsolute())
		return SetError(error, QStringLiteral("Invalid capture identity or OBS output path"));
	const QFileInfo source(obsOutputPath);
	if (!source.isFile() || !source.isReadable() || source.size() < 1)
		return SetError(error, QStringLiteral("Completed OBS output is unavailable"));
	VerifiedMedia media;
	if (!ProbeMp4(obsOutputPath, &media, error))
		return false;
	const QString destination = QDir(catalogRoot).filePath(relativePath);
	if (QFileInfo::exists(destination))
		return SetError(error, QStringLiteral("Target media path already exists; recovery required"));
	QFile input(obsOutputPath);
	if (!input.open(QIODevice::ReadOnly))
		return SetError(error, QStringLiteral("Cannot read completed OBS output"));
	QSaveFile output(destination);
	if (!output.open(QIODevice::WriteOnly))
		return SetError(error, QStringLiteral("Cannot create catalog media copy"));
	QCryptographicHash sourceHash(QCryptographicHash::Sha256);
	qint64 copied = 0;
	while (!input.atEnd()) {
		const QByteArray block = input.read(1024 * 1024);
		if (block.isEmpty() && input.error() != QFile::NoError) {
			output.cancelWriting();
			return SetError(error, QStringLiteral("OBS output changed or became unreadable during copy"));
		}
		if (output.write(block) != block.size()) {
			output.cancelWriting();
			return SetError(error, QStringLiteral("Catalog media copy failed"));
		}
		sourceHash.addData(block);
		copied += block.size();
	}
	if (copied < 1 || copied != source.size() || !output.commit())
		return SetError(error, QStringLiteral("Catalog media copy did not complete"));
	QByteArray finalHash;
	if (!HashFile(destination, &finalHash, error) || finalHash != sourceHash.result())
		return SetError(error, QStringLiteral("Copied media content failed SHA-256 verification"));
	if (!ProbeMp4(destination, &media, error))
		return false;
	media.clipId = clipId;
	media.relativePath = relativePath;
	media.sizeBytes = copied;
	media.sha256 = QString::fromLatin1(finalHash.toHex());
	*result = media;
	return true;
}
