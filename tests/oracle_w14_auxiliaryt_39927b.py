#!/usr/bin/env python3
"""Differential oracle for gate w14_auxiliaryt_39927b (agent/auxiliary_task_config.py).

Loads the LIVE module with importlib.spec_from_file_location, recomputes each
fixture's expected value from the live functions, and compares it to the C
harness `out`. sys.path/quarry + parent-package bootstrap included.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parent.parent
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "agent" / "auxiliary_task_config.py").is_file():
    QUARRY = Path("C:/Users/eman5/hermes-agent")
sys.path.insert(0, str(QUARRY))

for _pkg in ("agent", "hermes_cli"):
    if _pkg not in sys.modules:
        _m = types.ModuleType(_pkg)
        _m.__path__ = []
        sys.modules[_pkg] = _m

_SRC = str(QUARRY / "agent" / "auxiliary_task_config.py")
_SPEC = importlib.util.spec_from_file_location("w14_auxiliaryt_live", _SRC)
mod = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = mod
_SPEC.loader.exec_module(mod)


def expected(rec):
    f = rec.get("func")
    if f == "authoritative_auxiliary_task":
        return str(mod.authoritative_auxiliary_task(rec.get("task")))
    if f == "core_rooted":
        return bool(mod._core_rooted(rec.get("base"), rec.get("entries") or {}))
    if f == "layer_over_inherited":
        return mod._layer_over_inherited(rec.get("inherited") or {}, rec.get("user") or {})
    raise RuntimeError("unknown func " + str(f))


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        want = expected(rec)
        got = rec.get("out")
        if got != want:
            mism += 1
            if mism <= 10:
                print("MISMATCH %s\n  got : %s\n  want: %s" %
                      (rec.get("func"), json.dumps(got, ensure_ascii=False),
                       json.dumps(want, ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w14_auxiliaryt_39927b: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
