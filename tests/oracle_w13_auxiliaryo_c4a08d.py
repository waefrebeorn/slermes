#!/usr/bin/env python3
"""Differential oracle for agent/auxiliary_openai_url.py (gate w13_auxiliaryo_c4a08d).

Imports the LIVE module with a stub `utils` wired to the live utils/urls.py and
a faithful stub of hermes_cli.auth.normalize_actual_base_url (the live module
imports both lazily). sys.modules registration + parent-package bootstrap keep
the local imports resolving without dragging in the whole CLI.
"""
import importlib.util
import json
import sys
import types
from urllib.parse import urlparse

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


# utils.urls is the real source of base_url_hostname / base_url_host_matches.
uu = _load("live_utils_urls", QUARRY + "/utils/urls.py")
utils_stub = types.ModuleType("utils")
utils_stub.base_url_hostname = uu.base_url_hostname
utils_stub.base_url_host_matches = uu.base_url_host_matches
sys.modules["utils"] = utils_stub


def normalize_actual_base_url(base_url: str) -> str:
    url = str(base_url or "").strip().rstrip("/")
    if not url:
        return "https://api.actual.inc/v1"
    try:
        parsed = urlparse(url)
        host = (parsed.hostname or "").lower().rstrip(".")
        path = parsed.path.rstrip("/")
    except Exception:
        return url
    if path in {"", "/"} and (host == "api.actual.inc" or
                              host in {"localhost", "127.0.0.1", "::1", "0.0.0.0"}):
        return url + "/v1"
    return url


hc = types.ModuleType("hermes_cli")
hc.__path__ = []
auth_stub = types.ModuleType("hermes_cli.auth")
auth_stub.normalize_actual_base_url = normalize_actual_base_url
sys.modules["hermes_cli"] = hc
sys.modules["hermes_cli.auth"] = auth_stub
hc.auth = auth_stub

live = _load("aux_openai_url_live", QUARRY + "/agent/auxiliary_openai_url.py")


def compute(r):
    f = r.get("func")
    if f == "dual":
        return live._is_dual_surface_anthropic_host(r.get("url", ""))
    if f == "to_openai":
        return live._to_openai_base_url(r.get("base_url", ""))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_auxiliaryo_c4a08d: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
