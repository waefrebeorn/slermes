#!/usr/bin/env python3
"""Differential oracle for agent/checkpoints/refs.py (gate w22_refs_0a533f).

resolve_checkpoint_ref is pure, so the live function runs unmodified. The seam
is only the input shape: `checkpoints` is the already-decoded `list_checkpoints`
array (the C port carries it as a JSON array), and an absent key reads as no
checkpoints at all.

The live call is wrapped so a raise becomes a payload instead of a traceback:
  ValueError        -> {"ok": false, "type": "ValueError", "error": str(exc)}
  anything else     -> {"ok": false, "type": type(exc).__name__, "error": str(exc)}
  a returned string -> {"ok": true, "value": <value>}
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "agent" / "checkpoints" / "refs.py"
GATE = "w22_refs_0a533f"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("refs_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["refs_live"] = live
_spec.loader.exec_module(live)

WANTED = {"resolve"}


def resolve(checkpoints, ref):
    cps = checkpoints if isinstance(checkpoints, list) else []
    try:
        value = live.resolve_checkpoint_ref(cps, ref)
    except ValueError as exc:
        return {"ok": False, "error": str(exc), "type": "ValueError"}
    except Exception as exc:  # noqa: BLE001 - the port reports the raise shape
        return {"ok": False, "error": str(exc), "type": type(exc).__name__}
    return {"ok": True, "value": value}


def expected(r):
    f = r.get("func")
    if f == "resolve":
        return resolve(r.get("checkpoints"), r.get("ref"))
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
