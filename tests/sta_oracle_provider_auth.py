#!/usr/bin/env python3
"""
sta_oracle_provider_auth.py — oracle for t_port_provider_auth.c.

Enumerates the LIVE hermes_cli.auth.PROVIDER_REGISTRY and emits one JSON line
per entry, same shape as the C harness:
    {"name": <key>, "auth_type": <int>}
where the int maps the Python auth_type string to the C provider_auth_type_t
enum (see lib/libproviderauth/provider_auth.h):
    0 UNKNOWN, 1 api_key, 2 oauth_device_code, 3 oauth_external,
    4 aws_sdk, 5 external_process, 6 oauth_minimax.

The runner diffs the two; any missing/extra/renamed key or wrong auth_type in
the C table is caught.
"""

import sys
import os
import importlib.util
import json as _json

ENUM = {
    "api_key": 1,
    "oauth_device_code": 2,
    "oauth_external": 3,
    "aws_sdk": 4,
    "external_process": 5,
    "oauth_minimax": 6,
}
DEFAULT_ENUM = 0  # anything unmapped -> UNKNOWN (as the C side would)


def _load_auth():
    # Deterministic LIVE-Python resolution: prefer the canonical dev repo
    # (parent of slermes/) over any installed/stale copy on sys.path
    # (e.g. ~/.hermes/hermes-agent), which would manufacture false FAPs.
    _repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    if _repo not in sys.path:
        sys.path.insert(0, _repo)
    for base in sys.path:
        cand = os.path.join(base, "hermes_cli", "auth.py")
        if os.path.exists(cand):
            try:
                spec = importlib.util.spec_from_file_location("live_auth", cand)
                mod = importlib.util.module_from_spec(spec)
                sys.modules["live_auth"] = mod
                spec.loader.exec_module(mod)
                return mod
            except Exception:
                continue
    import hermes_cli.auth as mod  # type: ignore
    return mod


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_provider_auth.py <unused>\n")
        return 2
    auth = _load_auth()
    REGISTRY = auth.PROVIDER_REGISTRY
    for name in sorted(REGISTRY):
        pc = REGISTRY[name]
        at = getattr(pc, "auth_type", None)
        val = ENUM.get(at, DEFAULT_ENUM)
        sys.stdout.write('{"name":%s,"auth_type":%d}\n' % (_json.dumps(name), val))
    return 0


if __name__ == "__main__":
    sys.exit(main())
