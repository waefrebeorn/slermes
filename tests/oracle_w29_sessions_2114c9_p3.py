#!/usr/bin/env python3
"""Differential oracle for gate w29_sessions_2114c9_p3.

Truth: the SQL fragment builders the session module embeds (sessions.py's
_LOCAL_CHAT/_DESCENDANTS_SQL, _store/sql_common.py's recency expression,
_schema_ddl.py's json_extract / reset-child helpers) and the _title_refusals
classification of the store's ValueError text.  Both f-string builders are
re-derived here from the live constants so the comparison is byte-for-byte.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_sessions_2114c9_p3"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.conversation import sessions as live  # noqa: E402
from persistence.sqlite import _schema_ddl as ddl  # noqa: E402

WANTED = {
    "sql_literal", "json_extract", "legacy_reset_child", "reset_child", "descendants",
    "session_last_active", "local_chat", "owner_filter", "roster_sqls",
    "last_active_select", "title_refusal", "fragments",
}


def hidden_sources():
    return ",".join("?" * len(live.INTERNAL_LISTING_SOURCES))


def owner_filter(r):
    if r.get("operator"):
        return "", ()
    return "user_id=? AND ", (r.get("subject"),)


def roster_sqls(r):
    filt, _params = owner_filter(r)
    hidden = hidden_sources()
    canonical = f"SELECT * FROM sessions WHERE title=? AND {live._LOCAL_CHAT}"
    latest = (f"SELECT * FROM sessions WHERE {filt}{live._LOCAL_CHAT} AND archived=0 AND hidden=0 "
              f"AND COALESCE(source,'') NOT IN ({hidden}) "
              "ORDER BY COALESCE(last_activity_at,started_at) DESC LIMIT 1")
    worker = ("SELECT id, source, title, COALESCE(last_activity_at, started_at) AS last_active "
              "FROM sessions "
              f"WHERE COALESCE(source,'') IN ({hidden}) AND archived=0 AND hidden=0 "
              "ORDER BY last_active DESC LIMIT 1")
    return {"canonical": canonical, "latest": latest, "worker": worker}


def title_refusal(r):
    text = r.get("text")
    if text is None:
        return "pass"
    text = str(text)
    held = live._TITLE_HELD.fullmatch(text)
    if held:
        return {"kind": "conflict", "title": held["title"], "holder": held["holder"]}
    if text.startswith(live._CANONICAL_TITLE_LOCKED):
        return "canonical"
    return "pass"


def expected(r):
    f = r.get("func")
    if f == "sql_literal":
        return ddl._sql_literal(r.get("text") or "")
    if f == "json_extract":
        return ddl._sql_json_extract(r.get("expression") or "", r.get("path") or "")
    if f == "legacy_reset_child":
        return ddl._legacy_reset_child_sql(r.get("alias") or "", r.get("reasons_sql") or "")
    if f == "reset_child":
        return ddl._RESET_CHILD_SQL.format(a=r.get("alias") or "s")
    if f == "descendants":
        return live._DESCENDANTS_SQL
    if f == "session_last_active":
        return live._sql_session_last_active(r.get("alias") or "s")
    if f == "local_chat":
        return live._LOCAL_CHAT
    if f == "owner_filter":
        sql, params = owner_filter(r)
        return {"sql": sql, "params": list(params)}
    if f == "roster_sqls":
        return roster_sqls(r)
    if f == "last_active_select":
        return f"SELECT {live._sql_session_last_active('s')} FROM sessions s WHERE s.id = ?"
    if f == "title_refusal":
        return title_refusal(r)
    if f == "fragments":
        return {
            "hidden_sources": hidden_sources(),
            "reset_end_reasons": list(ddl._RESET_END_REASONS),
            "internal_sources": list(live.INTERNAL_LISTING_SOURCES),
            "legacy_prefix": live.LEGACY_PREFIX,
            "epoch_window": [0.0, 4200000000.0],
        }
    raise AssertionError("unknown func %r" % (f,))


def norm(v):
    return json.loads(json.dumps(v, sort_keys=True))


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
            py = norm(expected(r))
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n py=%r\n  c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism or not cases else 0)


if __name__ == "__main__":
    main()
