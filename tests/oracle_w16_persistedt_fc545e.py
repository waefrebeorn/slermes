#!/usr/bin/env python3
"""Differential oracle for gate w16_persistedt_fc545e (agent/persisted_turn_receipt.py).

Loads the LIVE module via spec_from_file_location with a stubbed
agent.context_compressor for _DB_PERSISTED_MARKER. prefix_row_matches is pinned
to exact equality (before == after), matching the C deep-JSON-equality seam.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if not (Path(QUARRY) / "agent" / "persisted_turn_receipt.py").is_file():
    QUARRY = str(Path(__file__).resolve().parent.parent.parent / "hermes-agent")
sys.path.insert(0, QUARRY)
for _pkg in ("agent", "hermes_cli"):
    if _pkg not in sys.modules:
        _m = types.ModuleType(_pkg)
        _m.__path__ = [str(Path(QUARRY) / _pkg)]
        sys.modules[_pkg] = _m

_cc = types.ModuleType("agent.context_compressor")
_cc._DB_PERSISTED_MARKER = "_db_persisted"
sys.modules["agent.context_compressor"] = _cc

_SPEC = importlib.util.spec_from_file_location(
    "w16_persisted_receipt_live", str(Path(QUARRY) / "agent" / "persisted_turn_receipt.py"))
mod = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = mod
_SPEC.loader.exec_module(mod)


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("bool", x)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, list): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def expected(rec):
    f = rec.get("func")
    if f == "committed_row_id":
        return mod.committed_row_id(rec.get("message"))
    if f == "receipt":
        return mod.persisted_turn_receipt(
            rec.get("messages"), int(rec.get("start") or 0), rec.get("history") or [],
            rec.get("raw") or "", rec.get("status") or "",
            compression_unchanged=bool(rec.get("compression_unchanged")),
            prefix_row_matches=lambda a, b: a == b)
    raise RuntimeError("unknown func " + str(f))


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line: continue
        rec = json.loads(line)
        n += 1
        want = expected(rec)
        got = rec.get("out")
        if _canon(got) != _canon(want):
            mism += 1
            if mism <= 10:
                print("MISMATCH %s\n  got : %s\n  want: %s" %
                      (rec.get("func"), json.dumps(got, ensure_ascii=False),
                       json.dumps(want, ensure_ascii=False)), file=sys.stderr)
    print("ORACLE w16_persistedt_fc545e: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
