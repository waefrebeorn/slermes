#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/api_server_request_options.py
(gate w21_apiserverr_960557).

The module is pure: both readers take the decoded model_options dict and only
lazily import three facade helpers plus the `_REQUEST_OPTION_MISSING` sentinel.
So the oracle imports the module itself and calls the real functions, and takes
the effort ladder and the sentinel from the real facade module -- nothing is
stubbed. `_REQUEST_OPTION_MISSING` has no JSON form, so it is canonicalized to
{"present": false, "value": null} exactly as the C side carries it.
"""
import importlib.util
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "gateway" / "platforms" / "api_server_request_options.py"
GATE = "w21_apiserverr_960557"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("api_server_request_options_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["api_server_request_options_live"] = live
_spec.loader.exec_module(live)

import gateway.platforms.api_server as facade  # noqa: E402  (live helper home)

WANTED = {"reasoning", "service_tier", "efforts"}


def expected(r):
    f = r.get("func")
    if f == "reasoning":
        return live._request_reasoning_config(r.get("model_options"))
    if f == "service_tier":
        v = live._request_service_tier(r.get("model_options"))
        if v is facade._REQUEST_OPTION_MISSING:
            return {"present": False, "value": None}
        return {"present": True, "value": v}
    if f == "efforts":
        return sorted(facade._REASONING_EFFORTS)
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
        except Exception as exc:  # noqa: BLE001 - a live raise is drift, not a pass
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
