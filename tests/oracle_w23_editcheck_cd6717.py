#!/usr/bin/env python3
"""Differential oracle for agent/lsp/edit_check.py (gate w23_editcheck_cd6717).

The verifier under test is the delta filter the post-write check runs on the
service loop.  Its seams are the language server, the loop, the deferred
delivery and the clock, so the fixture supplies what those would answer:

  * the two snapshots are the bytes the write held (``pre_text`` /
    ``post_text``) -- no file is read here;
  * ``time.monotonic`` is replaced by the fixture's ``now`` (``live.time`` is
    swapped for a namespace, which is exactly the seam ``inline_wait`` uses);
  * the diagnostics are the server's published verdicts, handed over as data.

Everything else is the live code: ``inline_wait``, ``_digest``,
``EditChecks.remember``/``recall``, ``_diagnostic_key`` from
``agent.lsp.client`` and ``shift_baseline``/``build_line_shift`` from
``agent.lsp.range_shift`` in the same composition ``_check`` uses.

One harness convention, applied identically on both sides: Python joins the
identity key's fields with a literal NUL, which no JSON string can carry, so
both sides render those separators as 0x1f.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "agent" / "lsp" / "edit_check.py"
GATE = "w23_editcheck_cd6717"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("edit_check_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["edit_check_live"] = live
_spec.loader.exec_module(live)

from agent.lsp.range_shift import build_line_shift, shift_baseline  # noqa: E402

WANTED = {"consts", "wait", "digest", "key", "shift", "introduced", "cache"}
SEP = "\x1f"


def key_of(diag):
    return live._diagnostic_key(diag).replace("\x00", SEP)


def do_wait(req):
    now = req.get("now")
    saved = live.time
    live.time = types.SimpleNamespace(monotonic=lambda: now)
    try:
        return live.inline_wait(req.get("started"))
    finally:
        live.time = saved


def do_shift(req):
    baseline = req.get("baseline") or []
    shift = build_line_shift(req.get("pre"), req.get("post"))
    return shift_baseline(baseline, shift)


def do_introduced(req):
    diags = req.get("diags") or []
    pre, post = req.get("pre"), req.get("post")
    if pre is None or post is None:
        return list(diags)
    shift = build_line_shift(pre, post)
    seen = {key_of(d) for d in shift_baseline(req.get("baseline") or [], shift)}
    return [d for d in diags if key_of(d) not in seen]


def do_cache(req):
    checks = live.EditChecks()
    results = []
    for op in req.get("ops") or []:
        kind = op.get("op") if isinstance(op, dict) else None
        if kind == "remember":
            checks.remember(op.get("path"), op.get("text", ""), op.get("diags") or [])
            results.append(None)
        elif kind == "recall":
            results.append(checks.recall(op.get("path"), op.get("text", "")))
        else:
            results.append(None)
    return {"results": results, "size": len(checks.verdicts)}


def expected(req):
    func = req.get("func")
    if func == "consts":
        return {"write_budget": live.WRITE_BUDGET, "inline_grace": live.INLINE_GRACE,
                "verdict_cap": live._VERDICT_CAP}
    if func == "wait":
        return do_wait(req)
    if func == "digest":
        return live._digest(req.get("text", ""))
    if func == "key":
        return key_of(req.get("diag"))
    if func == "shift":
        return do_shift(req)
    if func == "introduced":
        return do_introduced(req)
    if func == "cache":
        return do_cache(req)
    raise AssertionError("unknown func %r" % (func,))


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
