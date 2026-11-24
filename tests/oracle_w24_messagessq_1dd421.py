#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/messages_sql.py
(gate w24_messagessq_1dd421).

The live module is imported as a package module and its pure helpers are called
directly: ``_coerce_timestamp``, ``_parse_tool_calls``, ``_tool_calls_count``,
``_tool_calls_len``, the ``_MESSAGE_WRITE_COLUMNS`` table Python derives from the
INSERT by regex, the ``_MESSAGE_SCHEMA_KEYS`` union, and every statement literal.

The connection is the seam and is never created: these are the value decisions a
message write makes before it touches SQLite.  Logging is silenced so the
readers' "corrupt timestamp" warnings do not pollute the diff.
"""
import json
import logging
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_messagessq_1dd421"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.conversation._store import messages_sql as live  # noqa: E402

logging.disable(logging.CRITICAL)

WANTED = {"write_columns", "schema_keys", "coerce_ts", "parse_tool_calls",
          "tool_calls_count", "tool_calls_len", "constants"}

LITERALS = (
    ("insert_message_sql", "_INSERT_MESSAGE_SQL"),
    ("bump_generation_sql", "_BUMP_GENERATION_SQL"),
    ("turn_lease_row_sql", "_TURN_LEASE_ROW_SQL"),
    ("delete_compression_lock_sql", "_DELETE_COMPRESSION_LOCK_SQL"),
    ("display_active_clause", "_DISPLAY_ACTIVE_CLAUSE"),
    ("display_meta_row_sql", "_DISPLAY_META_ROW_SQL"),
    ("display_index_missing_sql", "_DISPLAY_INDEX_MISSING_SQL"),
    ("active_ids_sql", "_ACTIVE_IDS_SQL"),
    ("live_identity_sql", "_LIVE_IDENTITY_SQL"),
    ("set_counters_sql", "_SET_COUNTERS_SQL"),
    ("reset_counters_sql", "_RESET_COUNTERS_SQL"),
    ("set_display_meta_sql", "_SET_DISPLAY_META_SQL"),
    ("archive_active_sql", "_ARCHIVE_ACTIVE_SQL"),
    ("shadowed_checkpoint_rows_sql", "_SHADOWED_CHECKPOINT_ROWS_SQL"),
    ("set_codex_reasoning_sql", "_SET_CODEX_REASONING_SQL"),
)


def write_columns(r):
    return list(live._MESSAGE_WRITE_COLUMNS)


def schema_keys(r):
    return sorted(live._MESSAGE_SCHEMA_KEYS)


def coerce_ts(r):
    return live._coerce_timestamp(r.get("value"), r.get("default", 0.0))


def parse_tool_calls(r):
    return live._parse_tool_calls(r.get("tool_calls"))


def tool_calls_count(r):
    return live._tool_calls_count(r.get("tool_calls"))


def tool_calls_len(r):
    return live._tool_calls_len(r.get("raw"), r.get("scalar", 0))


def constants(r):
    return {key: getattr(live, attr) for key, attr in LITERALS}


def expected(r):
    f = r.get("func")
    if f == "write_columns":
        return write_columns(r)
    if f == "schema_keys":
        return schema_keys(r)
    if f == "coerce_ts":
        return coerce_ts(r)
    if f == "parse_tool_calls":
        return parse_tool_calls(r)
    if f == "tool_calls_count":
        return tool_calls_count(r)
    if f == "tool_calls_len":
        return tool_calls_len(r)
    if f == "constants":
        return constants(r)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r"
                  % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
