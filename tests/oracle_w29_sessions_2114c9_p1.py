#!/usr/bin/env python3
"""Differential oracle for gate w29_sessions_2114c9_p1.

Truth: persistence/sqlite/conversation/sessions.py (and the _parse_model_config
helper it imports).  The live module is imported through the package so its
relative imports resolve; no store, connection or holder is touched -- every
function under test is a value transform.

Compared here: _stripped, _json_value, _tool_pin, _ids, _removed, _listed,
_optional_list, _sql_limit, _last_active, _parse_model_config.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w29_sessions_2114c9_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from persistence.sqlite.conversation import sessions as live  # noqa: E402

WANTED = {
    "stripped", "json_value", "tool_pin", "ids", "removed", "listed",
    "optional_list", "sql_limit", "last_active", "parse_model_config",
}


def window_of(r):
    raw = r.get("window")
    if not isinstance(raw, dict):
        return live.SessionWindow(limit=20)
    return live.SessionWindow(limit=raw.get("limit", 20), offset=int(raw.get("offset", 0) or 0))


def expected(r):
    f = r.get("func")
    if f == "stripped":
        return live._stripped(r.get("value"))
    if f == "json_value":
        return live._json_value(r.get("text"))
    if f == "tool_pin":
        return live._tool_pin(r.get("text"))
    if f == "ids":
        return list(live._ids(r.get("values") or ()))
    if f == "removed":
        return list(live._removed(r.get("values") or ()))
    if f == "listed":
        return live._listed(r.get("values"))
    if f == "optional_list":
        return live._optional_list(r.get("values"))
    if f == "sql_limit":
        return live._sql_limit(window_of(r))
    if f == "last_active":
        return live._last_active(r.get("row") or {})
    if f == "parse_model_config":
        return live._parse_model_config(r.get("raw"))
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
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got),
                  file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism or not cases else 0)


if __name__ == "__main__":
    main()
