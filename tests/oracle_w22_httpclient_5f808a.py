#!/usr/bin/env python3
"""Differential oracle for gateway/platforms/_http_client_limits.py
(gate w22_httpclient_5f808a).

`_positive_env` and `platform_httpx_limits` both run live. The only seam is the
env read: each case sets the two env names on this process (and removes them
when the case omits them), then calls the module exactly as an adapter would.
The C side is handed the same raw text, so no process env is shared with it.
"""
import importlib.util
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "gateway" / "platforms" / "_http_client_limits.py"
GATE = "w22_httpclient_5f808a"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("http_client_limits_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["http_client_limits_live"] = live
_spec.loader.exec_module(live)

WANTED = {"pos_int", "pos_float", "limits"}
NAME_I = "HERMES_GATEWAY_HTTPX_MAX_KEEPALIVE"
NAME_F = "HERMES_GATEWAY_HTTPX_KEEPALIVE_EXPIRY"


def _set_env(name, value):
    if value is None:
        os.environ.pop(name, None)
    else:
        os.environ[name] = value


def pos_int(r):
    _set_env("PROBE_INT", r.get("raw"))
    return live._positive_env("PROBE_INT", r.get("default"), int,
                              allow_zero=bool(r.get("allow_zero")))


def pos_float(r):
    _set_env("PROBE_FLOAT", r.get("raw"))
    return live._positive_env("PROBE_FLOAT", r.get("default"), float)


def limits(r):
    _set_env(NAME_I, r.get("max_keepalive"))
    _set_env(NAME_F, r.get("expiry"))
    real = live.httpx
    if not r.get("have_httpx", True):
        live.httpx = None          # `except ImportError: httpx = None` seam
    try:
        got = live.platform_httpx_limits()
    finally:
        live.httpx = real
    if got is None:
        return None
    return {"max_keepalive_connections": got.max_keepalive_connections,
            "keepalive_expiry": got.keepalive_expiry}


def expected(r):
    f = r.get("func")
    if f == "pos_int":
        return pos_int(r)
    if f == "pos_float":
        return pos_float(r)
    if f == "limits":
        return limits(r)
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
