#!/usr/bin/env python3
"""Differential oracle for w14_webappprev_71707e (hermes_cli/webapp_previews.py).

FastAPI is replaced by a recording application so mount_preview_assets can be
driven and the registered paths/headers/media types observed on the live side.
"""
import asyncio
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = _mod(name)
    if not hasattr(m, "__path__"):
        m.__path__ = []
    return m


_pkg("hermes_cli")  # parent-package bootstrap


class _Resp:
    def __init__(self, content, headers=None, media_type=None):
        self.content = content
        self.headers = headers or {}
        self.media_type = media_type


_mod("fastapi", FastAPI=type("FastAPI", (), {}), Depends=lambda x: x)
_mod("fastapi.responses", HTMLResponse=_Resp, Response=_Resp)
_mod("hermes_cli.web_server_surface", route_on=lambda x: (lambda f: f))

SRC = Q + "/hermes_cli/webapp_previews.py"
spec = importlib.util.spec_from_file_location("hermes_cli.webapp_previews_w14_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


class App:
    def __init__(self):
        self.routes = []

    def get(self, path, dependencies=None):
        def deco(fn):
            self.routes.append((path, fn))
            return fn
        return deco


def route_manifest():
    app = App()
    live.mount_preview_assets(app)
    out = []
    for path, fn in app.routes:
        r = asyncio.run(fn())
        out.append({"path": path, "content": r.content,
                    "headers": r.headers, "media_type": r.media_type})
    return out


def canon(v):
    def conv(x):
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


def compute(r):
    f = r.get("func")
    if f == "preview_policy":
        return live.PREVIEW_POLICY
    if f == "guest_policy":
        return live.GUEST_POLICY
    if f == "preview_html":
        return live.PREVIEW_HTML
    if f == "preview_loader":
        return live.PREVIEW_LOADER
    if f == "route_manifest":
        return route_manifest()
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
    try:
        py = canon(compute(r))
    except Exception as exc:
        print(f"LIVE ERROR {r.get('func')} case={r.get('case')}: {exc!r}",
              file=sys.stderr)
        mism += 1
        continue
    got = canon(r.get("out"))
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py_len="
              f"{len(str(py))} c_len={len(str(got))}", file=sys.stderr)

print(f"ORACLE w14_webappprev_71707e: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
