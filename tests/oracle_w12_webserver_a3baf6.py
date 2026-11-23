#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_server_app.py (gate w12_webserver_a3baf6)."""
import asyncio
import contextlib
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg
_fa = types.ModuleType("fastapi")
_fa.FastAPI = object
sys.modules["fastapi"] = _fa
_pt = types.ModuleType("hermes_cli.pty_session")
_pt.run_reaper = lambda *a, **k: None
sys.modules["hermes_cli.pty_session"] = _pt

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m

live = load("w12_webserver_a3baf6_live", QUARRY + "/hermes_cli/web_server_app.py")

async def _drive(r):
    selected = []
    @contextlib.asynccontextmanager
    async def fake_http(app):
        selected.append("http")
        yield
    @contextlib.asynccontextmanager
    async def fake_standalone(app):
        selected.append("standalone")
        yield
    old_h, old_s = live.http_lifespan, live.standalone_lifespan
    live.http_lifespan, live.standalone_lifespan = fake_http, fake_standalone
    class St:
        pass
    st = St()
    if "gateway_runner" in r:
        st.gateway_runner = r["gateway_runner"]
    class App:
        pass
    app = App()
    app.state = st
    try:
        async with live.app_lifespan(app):
            pass
    finally:
        live.http_lifespan, live.standalone_lifespan = old_h, old_s
    return selected[0]

def compute(r):
    f = r.get("func")
    if f == "select_lifespan":
        return asyncio.run(_drive(r))
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
        py = compute(r)
    except Exception as exc:
        print("ORACLE w12_webserver_a3baf6: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w12_webserver_a3baf6 %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w12_webserver_a3baf6: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
