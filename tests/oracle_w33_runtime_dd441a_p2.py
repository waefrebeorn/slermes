#!/usr/bin/env python3
"""Differential oracle for persistence/authority/runtime.py — gate
w33_runtime_dd441a_p2 (the closed MutationCommand union and its predicates).

Everything the live module needs a store for (rows, the epoch, receipts) is a
seam that never runs.  What is compared is what the values decide:

  * ``parse_mutation`` — the operation/payload validation of every command of
    the union, the typed command's class name and its exact wire ``payload()``
    (absent keys left out, a present ``None``/``False`` kept where the command
    keeps it), or the ``invalid_params`` refusal
  * ``_reasoning_level`` — the string levels ``parse_reasoning_effort`` accepts
  * ``_editor_fingerprint`` — a dict whose only key is a str ``fingerprint``
  * ``_GENERATION_REQUIRED`` — which operations must carry a generation fence
  * ``METADATA_FIELDS`` / the module constant tables

Envelope note: a raise is reported as ``{"ok": false, "error": <reason>}``.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w33_runtime_dd441a_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

sys.path.append(str(Path(__file__).resolve().parent))  # hermes_seam_stubs
from hermes_seam_stubs import install as _install_seams  # noqa: E402

_install_seams()

from persistence.authority import runtime as live  # noqa: E402
from persistence.core.errors import AuthorityRefusal  # noqa: E402

WANTED = {"parse_mutation", "reasoning_level", "editor_fingerprint",
          "generation_required", "metadata_fields", "constants"}


def envelope(fn):
    try:
        return fn()
    except AuthorityRefusal as exc:
        return {"ok": False, "error": exc.reason}


def parse(req):
    command = live.parse_mutation(req.get("operation"), req.get("payload"))
    return {"ok": True, "command": type(command).__name__, "payload": command.payload()}


def reasoning_level(req):
    return live._reasoning_level(req.get("value"))


def editor_fingerprint(req):
    return live._editor_fingerprint(req.get("value"))


def generation_required(req):
    return req.get("operation") in live._GENERATION_REQUIRED


def metadata_fields():
    return {key: kind.__name__ for key, kind in live.METADATA_FIELDS.items()}


def constants():
    return {
        "reasoning_efforts": list(live._REASONING_EFFORTS),
        "generation_required": sorted(live._GENERATION_REQUIRED),
        "compress_fields": {key: kind.__name__ for key, kind in live._COMPRESS_FIELDS.items()},
        "metadata_fields": metadata_fields(),
    }


def expected(r):
    f = r.get("func")
    if f == "parse_mutation":
        return envelope(lambda: parse(r))
    if f == "reasoning_level":
        return envelope(lambda: reasoning_level(r))
    if f == "editor_fingerprint":
        return envelope(lambda: editor_fingerprint(r))
    if f == "generation_required":
        return envelope(lambda: generation_required(r))
    if f == "metadata_fields":
        return metadata_fields()
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
