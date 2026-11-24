#!/usr/bin/env python3
"""Differential oracle for persistence/sqlite/conversation/_store/compaction_display.py
(gate w21_compaction_8a06a1).

The live module imports only pure siblings (`session_rules.turns` for the
model-only metadata key, `persistence.sqlite._schema_ddl` for the JSON marker
lookup), so it is imported as-is and both functions run for real:

  * `not_a_later_tool_copy_sql(alias)` -- the SQL text, compared verbatim;
  * `later_tool_copy_ids(db, rows)` -- its row-level twin. `db` is only
    forwarded to `_is_model_only_row`, and that reader never touches the
    handle (its `holder` argument is unused), so `None` stands in for the open
    store: the row's own `display_metadata` column answers.

Python returns a set of ids; the port returns them ascending, so the expected
value is `sorted(set)`.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "persistence" / "sqlite" / "conversation" / "_store" / "compaction_display.py"
GATE = "w21_compaction_8a06a1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("compaction_display_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["compaction_display_live"] = live
_spec.loader.exec_module(live)


def expected(r):
    f = r.get("func")
    if f == "not_a_later_tool_copy_sql":
        return live.not_a_later_tool_copy_sql(r.get("alias", ""))
    if f == "later_tool_copy_ids":
        rows = r.get("rows")
        if rows is None:
            rows = []
        ids = live.later_tool_copy_ids(None, rows)
        return sorted(ids)
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"not_a_later_tool_copy_sql", "later_tool_copy_ids"}
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
