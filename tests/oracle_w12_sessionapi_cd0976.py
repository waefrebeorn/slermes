#!/usr/bin/env python3
"""Differential oracle for gate w12_sessionapi_cd0976 (gateway/session_api_media.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w12_sessionapi_cd0976"
sys.path.insert(0, QUARRY)

_gw = types.ModuleType("gateway")
_gw.__path__ = []
sys.modules["gateway"] = _gw

MEDIA = []
_ingress = types.ModuleType("gateway.session_ingress_media")
_ingress.restore_native_media = lambda media: list(MEDIA)
sys.modules["gateway.session_ingress_media"] = _ingress
_gw.session_ingress_media = _ingress

SRC = QUARRY + "/gateway/session_api_media.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    global MEDIA
    func = r.get("func")
    content = r.get("content")
    if func == "image_urls":
        return list(live._image_urls(content))
    if func == "restore_images":
        MEDIA = list(r.get("media_paths") or [])
        return live.restore_api_images(content, list(MEDIA))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
