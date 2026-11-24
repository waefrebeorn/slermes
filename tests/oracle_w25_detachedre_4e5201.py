#!/usr/bin/env python3
"""Differential oracle for
persistence/sqlite/conversation/_store/detached_results.py (gate w25_detachedre_4e5201).

Both appends end in ``_append_detached_result``, which is the DB seam: it is
replaced here by a recorder, so what the live functions decide is exactly what
is compared -- the display_kind, the lineage match SQL, the match parameters and
the (merged) metadata -- plus the ``ValueError`` seam when a delivery carries no
stable delegation id.  The module is loaded from its file so the
``persistence.sqlite`` package __init__ (which wants a resolvable home dir) never
runs; the source executed is the live upstream file.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w25_detachedre_4e5201"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_SRC = QUARRY / "persistence/sqlite/conversation/_store/detached_results.py"
_spec = importlib.util.spec_from_file_location("detached_results_live", _SRC)
live = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(live)

WANTED = {"delegation_delivery", "completion_note", "constants"}


def _record():
    captured = {}

    def rec(holder, session_id, content, kind, metadata, match_sql, params):
        captured["kind"] = kind
        captured["match_sql"] = match_sql
        captured["params"] = list(params)
        captured["metadata"] = metadata
        return 42

    return captured, rec


def _metadata(req):
    meta = req.get("metadata")
    return dict(meta) if isinstance(meta, dict) else meta


def delegation_delivery(req):
    captured, rec = _record()
    live._append_detached_result = rec
    try:
        live.append_delegation_delivery(None, req.get("session_id", ""), req.get("content", ""),
                                        _metadata(req))
    except ValueError as exc:
        return {"ok": False, "error": "ValueError", "message": str(exc)}
    return {"ok": True, "kind": captured["kind"], "match_sql": captured["match_sql"],
            "params": captured["params"], "metadata": captured["metadata"]}


def completion_note(req):
    captured, rec = _record()
    live._append_detached_result = rec
    live.append_completion_note(None, req.get("session_id", ""), req.get("admission_id"),
                                req.get("content", ""), req.get("display_kind"), _metadata(req))
    return {"ok": True, "kind": captured["kind"], "match_sql": captured["match_sql"],
            "params": captured["params"], "metadata": captured["metadata"]}


def _completion_match_sql():
    """The completion note's match condition is an inline literal: read it from
    the live source so the port cannot drift from the file it mirrors."""
    needle = "json_extract(m.display_metadata, '$.completion_admission') = ?"
    return needle if needle in _SRC.read_text(encoding="utf-8") else None


def constants():
    return {
        "lineage_match_sql": live._LINEAGE_MATCH_SQL,
        "delegation_delivery_match": live._DELEGATION_DELIVERY_MATCH,
        "completion_admission_match": _completion_match_sql(),
        "missing_delegation_id_message": "Delegation delivery requires a stable delegation_id",
    }


def expected(r):
    f = r.get("func")
    if f == "delegation_delivery":
        return delegation_delivery(r)
    if f == "completion_note":
        return completion_note(r)
    if f == "constants":
        return constants()
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
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
