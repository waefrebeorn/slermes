#!/usr/bin/env python3
"""Differential oracle for plugins/platforms/matrix/session_proxy.py
(gate w22_sessionpro_f01e6e).

Two reads are seams and are injected, everything else runs live:

  socks     -- live `matrix_redaction_proxy`'s SOCKS test, through the live
               module's own expression (the module is imported for real).
  redaction -- live `matrix_redaction_proxy`, with
                 * `_socks_connector_missing` bound to the fixture's
                   `socks_available` (the aiohttp_socks import is a seam),
                 * `gateway_trust_env` bound to the fixture's `trust_env`
                   (config.yaml is a seam),
                 * os.environ reduced to the fixture's `env` map (the process
                   environment is the seam `env_proxy_for_redaction` reads).
               The environment *policy* -- which spelling wins, unsafe-first,
               the NO_PROXY bypass -- stays live.

Every proxy variable is cleared (any case) before the fixture's map is
installed so a developer's real proxy cannot leak into a case.
"""
import importlib
import json
import os
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w22_sessionpro_f01e6e"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

live = importlib.import_module("plugins.platforms.matrix.session_proxy")

PROXY_KEYS = {f"{scheme}_proxy" for scheme in
              ("https", "http", "all", "wss", "ws", "socks", "grpc")}
WANTED = {"socks", "redaction"}


def clear_env():
    for key in [k for k in os.environ if k.lower() in PROXY_KEYS | {"no_proxy"}]:
        del os.environ[key]


def install_env(env):
    clear_env()
    for key, value in (env or {}).items():
        if isinstance(value, str):
            os.environ[key] = value


def expected(r):
    f = r.get("func")
    proxy_url = r.get("proxy_url")
    if f == "socks":
        return 1 if (bool(proxy_url) and proxy_url.split("://")[0].lower().startswith("socks")) else 0

    socks_available = bool(r.get("socks_available"))
    trust_env = bool(r.get("trust_env"))
    original_missing = live._socks_connector_missing
    original_trust = live.gateway_trust_env
    live._socks_connector_missing = lambda: not socks_available
    live.gateway_trust_env = lambda: trust_env
    install_env(r.get("env"))
    try:
        return live.matrix_redaction_proxy(proxy_url, r.get("homeserver"))
    finally:
        live._socks_connector_missing = original_missing
        live.gateway_trust_env = original_trust
        clear_env()


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
