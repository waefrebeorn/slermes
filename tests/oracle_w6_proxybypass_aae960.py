#!/usr/bin/env python3
"""Oracle for upstream Hermes agent/proxy_bypass.py (gate w6_proxybypass_aae960)."""
import importlib.util
import json
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

spec = importlib.util.spec_from_file_location(
    "live_proxy_bypass", QUARRY + "/agent/proxy_bypass.py")
live = importlib.util.module_from_spec(spec)
sys.modules["live_proxy_bypass"] = live
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
    if func == "split":
        py = dict(zip(("host", "port"), live.split_host_port(r.get("value", ""))))
    elif func == "entries":
        py = live.no_proxy_entries(r.get("value", ""))
    elif func == "loopback":
        py = live.is_loopback_host(r.get("host", ""))
    elif func == "match":
        port = r.get("port") if r.get("has_port") else None
        py = live.no_proxy_entry_matches(r.get("entry", ""), r.get("host", ""), port)
    elif func == "bypass":
        py = live.should_bypass_proxy(
            r.get("targets"), no_proxy_value=r.get("no_proxy_value"))
    elif func == "connect":
        py = live.loopback_connect_kwargs(r.get("url", ""))
    elif func == "request":
        py = live.loopback_request_kwargs(r.get("url", ""))
    else:
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w6_proxybypass_aae960: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
