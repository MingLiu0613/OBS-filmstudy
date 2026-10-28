# 0002: Pin the SQLite amalgamation for the catalog

Status: accepted for the foundation branch, pending native build verification.

OBS 33.0.0's locked Qt bundle lacks Qt6Sql on both Windows x64 and macOS arm64. The first QtSql catalog implementation therefore fails at CMake configuration on both runners. Use SQLite 3.46.1's official amalgamation in the Film Study module, verified against the official release SHA3-256. This keeps the OBS/Qt bundle unchanged and gives both platforms the same SQLite implementation for schema 2, STRICT tables and JSON functions. The UI may still use Qt Widgets and Core.

The catalog owns one native SQLite connection and a single-writer lock. Its migration is transactional. Existing catalogs with unknown versions are rejected without deletion. If a future version changes this schema, add a numbered migration and recovery tests; do not rebuild the database.
