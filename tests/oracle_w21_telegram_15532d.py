#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/telegram.py
(gate w21_telegram_15532d).

The live module imports cleanly against the quarry tree, so the oracle executes
it as-is: `_normalize_telegram_topic_profile_name` is the live function, and the
three query blocks are the live module constants.  The two helper expressions the
head f-string interpolates at import time (`_sql_preview_raw()`,
`_sql_session_last_active("s")`) are substituted back out with the same text the
fixture handed the C side, which is what makes the comparison a check of the
template rather than of `sql_common`.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
REL = "persistence/sqlite/conversation/_store/telegram.py"
SRC = QUARRY / REL
GATE = "w21_telegram_15532d"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("w21_telegram_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["w21_telegram_live"] = live
_spec.loader.exec_module(live)

from persistence.sqlite.conversation._store import sql_common as sc  # noqa: E402

_PREVIEW = sc._sql_preview_raw()
_LAST = sc._sql_session_last_active("s")


def py_str(v):
    return str(v)


def expected(r):
    f = r.get("func")
    if f == "normalize":
        return live._normalize_telegram_topic_profile_name(r.get("value"))
    if f == "params":
        try:
            return {"status": "ok",
                    "value": [str(r.get("user_id")),
                              live._normalize_telegram_topic_profile_name(r.get("profile")),
                              int(r.get("limit"))]}
        except Exception as exc:  # noqa: BLE001 - the harness reports the class only
            return {"status": "error", "type": type(exc).__name__}
    if f == "query":
        head = (live._UNLINKED_SELECT_HEAD.replace(_PREVIEW, r.get("preview") or "")
                .replace(_LAST, r.get("last_active") or ""))
        scoped = r.get("profile_scoped", True)
        return (head + (live._UNLINKED_SCOPE_CLAUSES if scoped else "")
                + live._UNLINKED_SELECT_TAIL)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"normalize", "params", "query"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
