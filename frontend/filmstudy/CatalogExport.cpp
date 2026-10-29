#include "Catalog.hpp"
#include "third_party/sqlite/sqlite3.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStorageInfo>
#include <QUuid>
#include <QVariant>

#include <limits>
#include <memory>

using namespace FilmStudy;

namespace {
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

QString Text(sqlite3_stmt *statement, int column)
{
	const auto *value = sqlite3_column_text(statement, column);
	return value ? QString::fromUtf8(reinterpret_cast<const char *>(value)) : QString();
}

QJsonValue NullableText(sqlite3_stmt *statement, int column)
{
	return sqlite3_column_type(statement, column) == SQLITE_NULL ? QJsonValue(QJsonValue::Null)
								 : QJsonValue(Text(statement, column));
}

bool Prepare(sqlite3 *database, const char *sql, Statement *statement, QString *error)
{
	sqlite3_stmt *raw = nullptr;
	if (sqlite3_prepare_v2(database, sql, -1, &raw, nullptr) != SQLITE_OK) {
		if (error)
			*error = QString::fromUtf8(sqlite3_errmsg(database));
		return false;
	}
	statement->reset(raw);
	return true;
}

bool Bind(sqlite3_stmt *statement, int index, const QString &value)
{
	const QByteArray bytes = value.toUtf8();
	return sqlite3_bind_text(statement, index, bytes.constData(), bytes.size(), SQLITE_TRANSIENT) == SQLITE_OK;
}

bool Fail(QString *error, const QString &message)
{
	if (error)
		*error = message;
	return false;
}

constexpr Qt::CaseSensitivity PathCaseSensitivity()
{
#ifdef Q_OS_WIN
	return Qt::CaseInsensitive;
#else
	return Qt::CaseSensitive;
#endif
}

bool IsWithin(const QString &path, const QString &directory)
{
	const QString normalizedPath = QDir::fromNativeSeparators(path);
	const QString normalizedDirectory = QDir::fromNativeSeparators(directory);
	return normalizedPath.startsWith(normalizedDirectory + QLatin1Char('/'), PathCaseSensitivity());
}

QString UtcNow()
{
	return QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH:mm:ss.zzzZ");
}

bool CopyVerified(const QString &source, const QString &destination, qint64 expectedSize,
		  const QString &expectedHash, QString *error)
{
	QFile input(source);
	if (!input.open(QIODevice::ReadOnly))
		return Fail(error, QStringLiteral("Cannot open original media: %1").arg(input.errorString()));
	QSaveFile output(destination);
	if (!output.open(QIODevice::WriteOnly))
		return Fail(error, QStringLiteral("Cannot create export media: %1").arg(output.errorString()));
	QCryptographicHash hash(QCryptographicHash::Sha256);
	qint64 copied = 0;
	while (!input.atEnd()) {
		const QByteArray block = input.read(1024 * 1024);
		if (block.isEmpty() || output.write(block) != block.size()) {
			output.cancelWriting();
			return Fail(error, QStringLiteral("Export media read or write failed"));
		}
		hash.addData(block);
		copied += block.size();
	}
	if (copied != expectedSize || QString::fromLatin1(hash.result().toHex()) != expectedHash) {
		output.cancelWriting();
		return Fail(error, QStringLiteral("Original media no longer matches its immutable asset"));
	}
	if (!output.commit())
		return Fail(error, QStringLiteral("Cannot commit copied media: %1").arg(output.errorString()));
	return true;
}

bool WriteManifest(const QString &path, const QByteArray &bytes, QString *error)
{
	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
		return Fail(error, QStringLiteral("Cannot write frozen manifest"));
	return true;
}

struct ExportMedia {
	QString clipId;
	QString sourcePath;
	QString relativePath;
	QString sha256;
	qint64 sizeBytes = 0;
};
} // namespace

bool Catalog::ExportMatching(const QString &perspective, const QString &topicId,
			     const QString &assessment, const QString &destinationRoot,
			     QString *packagePath, QString *error)
{
	if (!database || !packagePath || destinationRoot.isEmpty() ||
	    (!perspective.isEmpty() && perspective != QStringLiteral("offense") &&
	     perspective != QStringLiteral("defense")) ||
	    (!assessment.isEmpty() && assessment != QStringLiteral("positive") &&
	     assessment != QStringLiteral("negative")))
		return Fail(error, QStringLiteral("Open a catalog and supply a supported export query"));
	const QString destination = QDir(destinationRoot).absolutePath();
	if (!QDir().mkpath(destination))
		return Fail(error, QStringLiteral("Cannot create export destination"));
	const QString catalogCanonical = QFileInfo(rootDirectory).canonicalFilePath();
	const QString destinationCanonical = QFileInfo(destination).canonicalFilePath();
	if (catalogCanonical.isEmpty() || destinationCanonical.isEmpty() ||
	    QDir::fromNativeSeparators(destinationCanonical).compare(
		QDir::fromNativeSeparators(catalogCanonical), PathCaseSensitivity()) == 0 ||
	    IsWithin(destinationCanonical, catalogCanonical))
		return Fail(error, QStringLiteral("Export destination must be outside the catalog"));
	QList<ClipMatch> preflight;
	if (!SearchComplete(perspective, topicId, assessment, &preflight, error))
		return false;
	if (preflight.isEmpty())
		return Fail(error, QStringLiteral("No ready analysed clips match this export query"));
	qint64 requiredBytes = 0;
	for (const ClipMatch &match : preflight) {
		const QFileInfo source(QDir(rootDirectory).filePath(match.relativePath));
		if (!source.isFile() || !IsWithin(source.canonicalFilePath(), catalogCanonical) ||
		    source.size() <= 0 ||
		    source.size() > std::numeric_limits<qint64>::max() - requiredBytes - 1024 * 1024)
			return Fail(error, QStringLiteral("A matching original MP4 is missing or unsafe"));
		requiredBytes += source.size();
	}
	const QStorageInfo storage(destination);
	if (!storage.isValid() || !storage.isReady() || storage.bytesAvailable() < requiredBytes + 1024 * 1024)
		return Fail(error, QStringLiteral("Export destination has insufficient free space"));

	const QString exportId = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();
	const QString packageName = QStringLiteral("filmstudy-export-%1").arg(exportId);
	const QString stagingName = QStringLiteral(".%1.pending").arg(packageName);
	const QString stagingPath = QDir(destination).filePath(stagingName);
	const QString finalPath = QDir(destination).filePath(packageName);
	const QString now = UtcNow();
	QJsonObject query{{"logic_version", 1}, {"mode", "analysed"}};
	if (!perspective.isEmpty()) query.insert("perspectives", QJsonArray{perspective});
	if (!topicId.isEmpty()) query.insert("topic_ids", QJsonArray{topicId});
	if (!assessment.isEmpty()) query.insert("assessments", QJsonArray{assessment});
	const QString queryJson = QString::fromUtf8(QJsonDocument(query).toJson(QJsonDocument::Compact));
	if (!Exec(QStringLiteral("BEGIN IMMEDIATE"), error))
		return false;
	bool ok = true;
	QList<ClipMatch> matches;
	QList<ExportMedia> media;
	QJsonArray clips;
	int analysisCount = 0;
	QString revision;
	bool hasRevision = false;
	if (!QueryValue("SELECT content_revision FROM catalog_info WHERE singleton=1", &revision,
			&hasRevision, error) || !hasRevision ||
	    !SearchComplete(perspective, topicId, assessment, &matches, error))
		ok = false;
	if (ok && matches.isEmpty())
		ok = Fail(error, QStringLiteral("No ready analysed clips match this export query"));
	if (ok)
		ok = ExecuteParams(
			"INSERT INTO export_jobs(id,catalog_id,state,destination_root,package_name,staging_name,"
			"query_json,query_version,metadata_schema_version,snapshot_catalog_revision,created_at,updated_at) "
			"VALUES (?,(SELECT catalog_id FROM catalog_info WHERE singleton=1),'preparing',?,?,?, ?,1,2,?,?,?)",
			{exportId, destination, packageName, stagingName, queryJson, revision, now, now}, error);

	const char *clipSql =
		"SELECT c.id,c.revision,c.title,c.capture_mode,c.created_at,c.source_name,c.source_url,"
		"c.source_note,c.period_code,c.game_clock_ms,g.id,g.name,g.game_date,g.season,"
		"g.home_team_name,g.away_team_name,m.id,m.size_bytes,m.sha256,m.duration_ms,"
		"m.video_codec,m.audio_codec,l.id,l.relative_path "
		"FROM clips c JOIN games g ON g.id=c.game_id "
		"JOIN media_assets m ON m.id=c.primary_asset_id "
		"JOIN media_locations l ON l.asset_id=m.id AND l.is_preferred=1 "
		"WHERE c.id=? AND c.capture_state='ready' AND c.deleted_at IS NULL "
		"AND l.availability='available'";
	const char *analysisSql =
		"SELECT a.id,a.revision,a.status,a.perspective,t.id,t.name,a.assessment,"
		"r.id,r.name,a.note,a.created_at,a.updated_at,a.reference_asset_id,"
		"a.range_start_ms,a.range_end_ms "
		"FROM analyses a LEFT JOIN topics t ON t.id=a.topic_id "
		"LEFT JOIN result_types r ON r.id=a.result_id "
		"WHERE a.clip_id=? AND a.deleted_at IS NULL ORDER BY a.created_at,a.id";
	const char *playersSql =
		"SELECT p.id,p.name,ap.role FROM analysis_players ap JOIN players p ON p.id=ap.player_id "
		"WHERE ap.analysis_id=? ORDER BY p.id";
	const char *aliasesSql =
		"SELECT alias FROM player_aliases WHERE player_id=? ORDER BY alias_key,id";
	for (int position = 0; ok && position < matches.size(); ++position) {
		const ClipMatch &match = matches[position];
		Statement row(nullptr, sqlite3_finalize);
		ok = Prepare(database, clipSql, &row, error) && Bind(row.get(), 1, match.clipId);
		if (!ok || sqlite3_step(row.get()) != SQLITE_ROW) {
			ok = Fail(error, QStringLiteral("Matching clip changed before export freeze"));
			break;
		}
		QJsonObject clip{{"clip_id", Text(row.get(), 0)},
				 {"revision", sqlite3_column_int(row.get(), 1)},
				 {"title", Text(row.get(), 2)}, {"capture_mode", Text(row.get(), 3)},
				 {"created_at", Text(row.get(), 4)}};
		QJsonObject source{{"name", NullableText(row.get(), 5)},
				   {"url", NullableText(row.get(), 6)}, {"note", Text(row.get(), 7)}};
		clip.insert("source", source);
		clip.insert("period", NullableText(row.get(), 8));
		QJsonValue gameClock(QJsonValue::Null);
		QJsonValue gameClockMs(QJsonValue::Null);
		if (sqlite3_column_type(row.get(), 9) != SQLITE_NULL) {
			const qint64 value = sqlite3_column_int64(row.get(), 9);
			gameClockMs = QJsonValue::fromVariant(QVariant::fromValue(value));
			gameClock = QStringLiteral("%1:%2").arg(value / 60000, 2, 10, QLatin1Char('0'))
					.arg((value / 1000) % 60, 2, 10, QLatin1Char('0'));
		}
		clip.insert("game_clock", gameClock);
		clip.insert("game_clock_ms", gameClockMs);
		clip.insert("game", QJsonObject{{"game_id", Text(row.get(), 10)},
						 {"name", Text(row.get(), 11)},
						 {"game_date", NullableText(row.get(), 12)},
						 {"season", NullableText(row.get(), 13)},
						 {"home_team", NullableText(row.get(), 14)},
						 {"away_team", NullableText(row.get(), 15)}});
		const QString assetId = Text(row.get(), 16);
		const qint64 size = sqlite3_column_int64(row.get(), 17);
		const QString sha = Text(row.get(), 18);
		const QString relative = QStringLiteral("media/%1.mp4").arg(match.clipId);
		clip.insert("file", QJsonObject{{"asset_id", assetId}, {"path", relative},
						 {"size_bytes", QJsonValue::fromVariant(QVariant::fromValue(size))},
						 {"sha256", sha},
						 {"duration_ms", QJsonValue::fromVariant(QVariant::fromValue(sqlite3_column_int64(row.get(), 19)))},
						 {"video_codec", Text(row.get(), 20)},
						 {"audio_codec", NullableText(row.get(), 21)}});
		const QString locationId = Text(row.get(), 22);
		const QString sourceRelative = Text(row.get(), 23);
		if (sourceRelative != QStringLiteral("media/%1.mp4").arg(match.clipId)) {
			ok = Fail(error, QStringLiteral("Original media path violates the catalog layout"));
			break;
		}
		media.append({match.clipId, QDir(rootDirectory).filePath(sourceRelative), relative, sha, size});
		ok = ExecuteParams(
			"INSERT INTO export_items(export_id,clip_id,asset_id,source_location_id,"
			"snapshot_clip_revision,position,destination_relpath,expected_size_bytes,expected_sha256) "
			"VALUES (?,?,?,?,?,?,?,?,?)",
			{exportId, match.clipId, assetId, locationId, QString::number(sqlite3_column_int(row.get(), 1)),
			 QString::number(position), relative, QString::number(size), sha}, error);
		QJsonArray matchedIds;
		for (const QString &analysisId : match.matchedAnalysisIds) {
			matchedIds.append(analysisId);
			if (ok)
				ok = ExecuteParams(
					"INSERT INTO export_item_matches(export_id,clip_id,analysis_id,"
					"snapshot_analysis_revision) SELECT ?,?,id,revision FROM analyses "
					"WHERE id=? AND clip_id=? AND status='complete' AND deleted_at IS NULL",
					{exportId, match.clipId, analysisId, match.clipId}, error);
			if (ok && sqlite3_changes(database) != 1)
				ok = Fail(error, QStringLiteral("Matched analysis changed before export freeze"));
		}
		clip.insert("matched_analysis_ids", matchedIds);
		QJsonArray analyses;
		Statement analysis(nullptr, sqlite3_finalize);
		ok = ok && Prepare(database, analysisSql, &analysis, error) && Bind(analysis.get(), 1, match.clipId);
		int analysisStep = SQLITE_DONE;
		while (ok && (analysisStep = sqlite3_step(analysis.get())) == SQLITE_ROW) {
			QJsonObject item{{"analysis_id", Text(analysis.get(), 0)},
					 {"revision", sqlite3_column_int(analysis.get(), 1)},
					 {"status", Text(analysis.get(), 2)},
					 {"perspective", NullableText(analysis.get(), 3)},
					 {"assessment", NullableText(analysis.get(), 6)},
					 {"note", Text(analysis.get(), 9)},
					 {"created_at", Text(analysis.get(), 10)},
					 {"updated_at", Text(analysis.get(), 11)},
					 {"reference_asset_id", Text(analysis.get(), 12)}};
			item.insert("topic", sqlite3_column_type(analysis.get(), 4) == SQLITE_NULL
						 ? QJsonValue(QJsonValue::Null)
						 : QJsonValue(QJsonObject{{"topic_id", Text(analysis.get(), 4)},
								      {"name", Text(analysis.get(), 5)}}));
			item.insert("actual_result", sqlite3_column_type(analysis.get(), 7) == SQLITE_NULL
						 ? QJsonValue(QJsonValue::Null)
						 : QJsonValue(QJsonObject{{"result_id", Text(analysis.get(), 7)},
								      {"name", Text(analysis.get(), 8)}}));
			item.insert("range", sqlite3_column_type(analysis.get(), 13) == SQLITE_NULL
						 ? QJsonValue(QJsonValue::Null)
						 : QJsonValue(QJsonObject{{"start_ms", sqlite3_column_int(analysis.get(), 13)},
								      {"end_ms", sqlite3_column_int(analysis.get(), 14)}}));
			QJsonArray players;
			Statement player(nullptr, sqlite3_finalize);
			ok = Prepare(database, playersSql, &player, error) && Bind(player.get(), 1, Text(analysis.get(), 0));
			int playerStep = SQLITE_DONE;
			while (ok && (playerStep = sqlite3_step(player.get())) == SQLITE_ROW) {
				QJsonArray aliases;
				Statement alias(nullptr, sqlite3_finalize);
				ok = Prepare(database, aliasesSql, &alias, error) && Bind(alias.get(), 1, Text(player.get(), 0));
				int aliasStep = SQLITE_DONE;
				while (ok && (aliasStep = sqlite3_step(alias.get())) == SQLITE_ROW)
					aliases.append(Text(alias.get(), 0));
				if (aliasStep != SQLITE_DONE)
					ok = Fail(error, QString::fromUtf8(sqlite3_errmsg(database)));
				players.append(QJsonObject{{"player_id", Text(player.get(), 0)},
							    {"name", Text(player.get(), 1)},
							    {"aliases", aliases}, {"role", Text(player.get(), 2)}});
			}
			if (playerStep != SQLITE_DONE)
				ok = Fail(error, QString::fromUtf8(sqlite3_errmsg(database)));
			item.insert("players", players);
			analyses.append(item);
			++analysisCount;
		}
		if (analysisStep != SQLITE_DONE && ok)
			ok = Fail(error, QString::fromUtf8(sqlite3_errmsg(database)));
		clip.insert("analyses", analyses);
		clips.append(clip);
	}
	QJsonObject manifest{{"schema_version", 2}, {"export_id", exportId},
			     {"exported_at", now},
			     {"application", QJsonObject{{"name", "Basketball Film Study OBS"},
							 {"version", "0.1.0"}, {"upstream_tag", "33.0.0"},
							 {"upstream_commit", "72e86655715d9a25d69ce446aa4dd44ba58e10d2"}}},
			     {"query", query}, {"clip_count", clips.size()},
			     {"analysis_count", analysisCount}, {"clips", clips},
			     {"catalog_id", CatalogId()}, {"snapshot_catalog_revision", revision.toInt()}};
	const QByteArray manifestBytes = QJsonDocument(manifest).toJson(QJsonDocument::Indented);
	if (ok)
		ok = ExecuteParams(
			"UPDATE export_jobs SET manifest_snapshot_json=?,clip_count=?,analysis_count=?,"
			"snapshot_frozen_at=?,state='copying',updated_at=? WHERE id=? AND state='preparing'",
			{QString::fromUtf8(manifestBytes), QString::number(clips.size()),
			 QString::number(analysisCount), now, now, exportId}, error);
	if (!ok || !Exec(QStringLiteral("COMMIT"), error)) {
		Exec(QStringLiteral("ROLLBACK"), nullptr);
		return false;
	}

	// The database transaction is closed before any large file I/O.
	if (!QDir(destination).mkdir(stagingName) || !QDir().mkpath(QDir(stagingPath).filePath("media")))
		ok = Fail(error, QStringLiteral("Cannot create export staging directory"));
	for (const ExportMedia &asset : media) {
		if (!ok)
			break;
		const QString sourceCanonical = QFileInfo(asset.sourcePath).canonicalFilePath();
		if (!IsWithin(sourceCanonical, catalogCanonical)) {
			ok = Fail(error, QStringLiteral("Original media location escapes catalog"));
			break;
		}
		ok = CopyVerified(asset.sourcePath, QDir(stagingPath).filePath(asset.relativePath),
				  asset.sizeBytes, asset.sha256, error);
		if (ok)
			ok = ExecuteParams("UPDATE export_items SET copy_state='verified',bytes_copied=?,"
					   "copied_at=? WHERE export_id=? AND clip_id=? AND copy_state='pending'",
					   {QString::number(asset.sizeBytes), UtcNow(), exportId, asset.clipId}, error);
	}
	if (ok)
		ok = WriteManifest(QDir(stagingPath).filePath("manifest.json"), manifestBytes, error);
	const QString manifestHash = QString::fromLatin1(
		QCryptographicHash::hash(manifestBytes, QCryptographicHash::Sha256).toHex());
	if (ok)
		ok = ExecuteParams("UPDATE export_jobs SET state='publishing',manifest_sha256=?,updated_at=? "
				   "WHERE id=? AND state='copying'",
				   {manifestHash, UtcNow(), exportId}, error);
	if (ok && (QFileInfo::exists(finalPath) || !QDir().rename(stagingPath, finalPath)))
		ok = Fail(error, QStringLiteral("Cannot publish export package atomically"));
	if (ok)
		ok = ExecuteParams("UPDATE export_jobs SET state='succeeded',completed_at=?,updated_at=? "
				   "WHERE id=? AND state='publishing'",
				   {UtcNow(), UtcNow(), exportId}, error);
	if (!ok) {
		// Preserve the frozen snapshot, staging/final files and error for a repair tool.
		ExecuteParams("UPDATE export_jobs SET state='failed',error_code='export_failed',"
			      "error_message=?,updated_at=? WHERE id=? AND state<>'succeeded'",
			      {error ? *error : QStringLiteral("Export failed"), UtcNow(), exportId}, nullptr);
		return false;
	}
	*packagePath = finalPath;
	return true;
}
