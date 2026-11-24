#!/usr/bin/env python3
"""Differential oracle for gateway/plugin_command_choice.py (gate w17_plugincomm_d95180).

gateway.config.Platform, hermes_cli.plugin_choices, gateway.session_server_requests
and tools.clarify_gateway are injected as deterministic stand-ins; the LIVE
_ask_viewers and chat_choices bodies run for every case.
"""
import asyncio
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

gw_pkg = types.ModuleType("gateway")
gw_pkg.__path__ = []
sys.modules["gateway"] = gw_pkg
config = types.ModuleType("gateway.config")
class Platform:
    LOCAL = "LOCAL"
config.Platform = Platform
sys.modules["gateway.config"] = config

cli_pkg = types.ModuleType("hermes_cli")
cli_pkg.__path__ = []
sys.modules["hermes_cli"] = cli_pkg
pc = types.ModuleType("hermes_cli.plugin_choices")
ABOUND = {"v": False}
BOUND = object()
pc.asker_bound = lambda: ABOUND["v"]
pc.bind_asker = lambda f: BOUND
sys.modules["hermes_cli.plugin_choices"] = pc

ssr = types.ModuleType("gateway.session_server_requests")
CAP = {"req": None}
RESULT = {"v": None}
def _avr(event_stream, sid, op, payload, timeout=None, qids=None):
    CAP["req"] = {"session_id": sid, "op": op, "payload": payload,
                  "timeout": timeout, "qids": list(qids or [])}
    return RESULT["v"]
ssr.ask_viewer_result = _avr
sys.modules["gateway.session_server_requests"] = ssr

tools_pkg = types.ModuleType("tools")
tools_pkg.__path__ = []
sys.modules["tools"] = tools_pkg
cg = types.ModuleType("tools.clarify_gateway")
TIMEOUT = {"v": 5}
cg.get_clarify_timeout = lambda: TIMEOUT["v"]
sys.modules["tools.clarify_gateway"] = cg

SRC = QUARRY + "/gateway/plugin_command_choice.py"
spec = importlib.util.spec_from_file_location("w17_plugincomm_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

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
    try:
        if func == "ask_viewers":
            RESULT["v"] = r.get("result")
            TIMEOUT["v"] = int(r.get("timeout", "0"))
            CAP["req"] = None
            ans = live._ask_viewers(types.SimpleNamespace(event_stream="ES"),
                                    r.get("session_id", ""), r.get("question", ""),
                                    list(r.get("labels") or []))
            py = {"answer": ans, "request": CAP["req"]}
        elif func == "chat_choices":
            ABOUND["v"] = bool(r.get("asker_bound"))
            src = types.SimpleNamespace(
                platform=Platform.LOCAL if r.get("is_local") else "other")
            async def go():
                return live.chat_choices(types.SimpleNamespace(), src, r.get("session_key", "sk"))
            ret = asyncio.run(go())
            py = "bound" if ret is BOUND else "none"
        else:
            continue
    except Exception as exc:
        mism += 1
        print("LIVE ERROR case=%s %r" % (r.get("case"), exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH case=%s func=%s\n  py=%r\n  c =%r" %
              (r.get("case"), func, py, got), file=sys.stderr)

print("ORACLE w17_plugincomm_d95180: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
