#!/usr/bin/env python3
"""Differential oracle for tools/mcp_oauth_provider.py (w11_mcpoauthp_5e630c gate).

Loads the LIVE module via spec_from_file_location with a parent-package
bootstrap and a stub for hermes_cli.version_info (needed at import time for the
module-level default User-Agent). SDK objects are SimpleNamespace doubles fed
from the fixture; only pure issuer/discovery/URL logic is exercised.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

# parent-package bootstrap + sys.modules registration.
hcli = types.ModuleType("hermes_cli"); hcli.__path__ = []
vi = types.ModuleType("hermes_cli.version_info")
vi.get_version_info = lambda: SimpleNamespace(base_version="TEST")
sys.modules["hermes_cli"] = hcli
sys.modules["hermes_cli.version_info"] = vi
hcli.version_info = vi

SRC = Q + "/tools/mcp_oauth_provider.py"
spec = importlib.util.spec_from_file_location("oauth_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["oauth_live"] = live
spec.loader.exec_module(live)


def mkns(d, attrs):
    if d is None:
        return None
    return SimpleNamespace(**{a: d.get(a) for a in attrs})


def drive(r):
    func = r.get("func")
    if func == "metadata_issuer":
        ctx = mkns(r.get("context"), ("oauth_metadata",))
        if ctx and ctx.oauth_metadata is not None:
            ctx.oauth_metadata = mkns(r["context"]["oauth_metadata"], ("issuer",))
        return live._metadata_issuer(ctx)
    if func == "asm_discovery_failure":
        resp = mkns(r.get("response"), ("request", "status_code", "url"))
        if resp and resp.request is not None:
            resp.request = mkns(r["response"]["request"], ("url",))
        return live._asm_discovery_failure(resp)
    if func == "with_discovery_context":
        exc = Exception(r.get("exc", ""))
        return str(live._with_discovery_context(exc, r.get("failures") or []))
    if func == "metadata_issued_by_origin":
        meta = mkns(r.get("metadata"), ("issuer",))
        resp = mkns(r.get("response"), ("request", "status_code", "url"))
        return live.metadata_issued_by_origin(meta, r.get("auth_server_url", ""), resp)
    if func == "google_offline_access_params":
        ctx = mkns(r.get("context"), ("oauth_metadata",))
        if ctx and ctx.oauth_metadata is not None:
            ctx.oauth_metadata = mkns(r["context"]["oauth_metadata"], ("issuer",))
        return live.google_offline_access_params(ctx)
    return "UNKNOWN_FUNC"


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    try:
        py = drive(r)
    except BaseException as exc:
        print(f"LIVE ERROR {r.get('func')}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w11_mcpoauthp_5e630c: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
