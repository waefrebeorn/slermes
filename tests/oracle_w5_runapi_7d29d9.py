#!/usr/bin/env python3
"""Oracle for upstream Hermes gateway/run_api.py (gate w5_runapi_7d29d9).

Three pure listener decisions are compared: _listener_public_url,
_embedded_chat_enabled and the setsockopt plan of _listener_socket (driven with
a fake socket so no listener is actually bound).
"""
import importlib.util
import json
import socket
import sys
import types
from types import SimpleNamespace

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)


def _pkg(name, path):
    m = types.ModuleType(name)
    m.__path__ = [path]
    sys.modules[name] = m


_pkg("gateway", QUARRY + "/gateway")
wpa = types.ModuleType("gateway.windows_proactor_accept")
wpa.install_proactor_accept_guard = lambda loop: None
sys.modules["gateway.windows_proactor_accept"] = wpa

spec = importlib.util.spec_from_file_location("gateway.run_api", QUARRY + "/gateway/run_api.py")
live = importlib.util.module_from_spec(spec)
sys.modules["gateway.run_api"] = live
spec.loader.exec_module(live)

EXCL = getattr(socket, "SO_EXCLUSIVEADDRUSE", -1)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    if func == "public_url":
        w = None if not r.get("webapp_present") else SimpleNamespace(public_url=r.get("public_url"))
        py = live._listener_public_url(w)
    elif func == "embedded":
        web = None
        if r.get("loaded_web"):
            web = SimpleNamespace()
            if r.get("has_flag"):
                web._DASHBOARD_EMBEDDED_CHAT_ENABLED = r.get("flag_val", True)
        sys.modules["hermes_cli.web_server"] = web
        py = live._embedded_chat_enabled()
    elif func == "socket":
        port = r.get("port", 0)
        is_nt = bool(r.get("is_nt"))
        dual = bool(r.get("dual_stack"))
        live._REUSE_FIXED_PORT = not is_nt
        live._EXCLUSIVE_FIXED_PORT = is_nt
        calls = []

        class FakeSock:
            def __init__(self, *args):
                pass

            def setsockopt(self, level, opt, val):
                calls.append((opt, val))

        old = live.socket.socket
        live.socket.socket = FakeSock
        try:
            live._listener_socket(socket.AF_INET, socket.SOCK_STREAM, 0, port, dual_stack=dual)
        finally:
            live.socket.socket = old
        py = {
            "dual_stack": any(o == socket.IPV6_V6ONLY and v == 0 for o, v in calls),
            "reuseaddr": any(o == socket.SO_REUSEADDR and v == 1 for o, v in calls),
            "exclusive": any(o == EXCL and v == 1 for o, v in calls),
        }
    else:
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w5_runapi_7d29d9: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
