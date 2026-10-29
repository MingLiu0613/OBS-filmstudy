"""Generate single SQLite statements for QtSql's one-statement exec API."""

import hashlib
import sqlite3
from pathlib import Path


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "migrations/0002_initial.sql"
OUTPUT = ROOT / "SchemaV2.generated.hpp"


def statements(sql):
    pending = ""
    for line in sql.splitlines(keepends=True):
        pending += line
        if sqlite3.complete_statement(pending):
            statement = pending.strip()
            if statement:
                yield statement
            pending = ""
    if pending.strip():
        raise ValueError("Incomplete SQL statement")


def main():
    # Git may check out CRLF on Windows and LF on CI/macOS. The migration
    # identity is the canonical LF source, not a host-specific checkout.
    source_bytes = SOURCE.read_bytes().replace(b"\r\n", b"\n")
    sql = source_bytes.decode("utf-8")
    parts = list(statements(sql))
    if len(parts) < 40:
        raise ValueError("Schema appears truncated")
    if not parts[0].endswith("PRAGMA foreign_keys = ON;"):
        raise ValueError("The first statement must enable foreign keys before BEGIN")
    generated = [
        "// Generated from migrations/0002_initial.sql; do not edit by hand.\n",
        "#pragma once\n",
        "#include <array>\n#include <string_view>\n",
        f'inline constexpr std::string_view filmStudySchemaV2Sha256 = "{hashlib.sha256(source_bytes).hexdigest()}";\n',
        f"inline constexpr std::array<std::string_view, {len(parts)}> filmStudySchemaV2 = {{\n",
    ]
    for part in parts:
        if ")FILMSTUDY\"" in part:
            raise ValueError("Raw string delimiter occurs in schema")
        generated.append(f'    R"FILMSTUDY({part})FILMSTUDY",\n')
    generated.append("};\n")
    OUTPUT.write_text("".join(generated), encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
