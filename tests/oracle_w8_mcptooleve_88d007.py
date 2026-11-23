#!/usr/bin/env python3
"""Differential oracle for tools/mcp_tool_events.py (gate w8_mcptooleve_88d007)."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
SRC = Q + "/tools/mcp_tool_events.py"
spec = importlib.util.spec_from_file_location("live_events_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["live_events_live"] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "events_enabled":
        return live.events_enabled(r.get("cfg", {}))
    if f == "callback_url":
        return live._callback_url(r.get("url_base", ""), r.get("route_name", ""))
    if f == "event_prompt":
        return live._event_prompt(r.get("server", ""), r.get("name", ""), r.get("instructions"))
    if f == "server_routes":
        return live._server_routes(r.get("subscriptions", {}), r.get("server", ""))
    if f == "subscribe_args_error":
        return live._subscribe_args_error(r.get("args", {}))
    if f == "subscribe_params":
        return live._subscribe_params(r.get("route_name", ""), r.get("route", {}),
                                      r.get("url_base", ""), r.get("state", {}))
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
    except Exception:
        py = None
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w8_mcptooleve_88d007: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
