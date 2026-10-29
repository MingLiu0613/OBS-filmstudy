#pragma once

#include "Catalog.hpp"

namespace FilmStudy {

// Copies an explicitly identified, closed OBS MP4 into the catalog. The copy
// and media probe happen outside a database transaction; PublishOriginal then
// records the verified immutable asset in one short transaction.
class MediaImporter final {
public:
	static bool Import(const QString &obsOutputPath, const QString &catalogRoot,
			   const QString &clipId, const QString &relativePath, VerifiedMedia *result,
			   QString *error);
};

} // namespace FilmStudy
