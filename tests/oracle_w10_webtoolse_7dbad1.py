#!/usr/bin/env python3
"""Differential oracle for gate w10_webtoolse_7dbad1 (tools/web_tools_extract.py)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
_tools = types.ModuleType("tools"); _tools.__path__ = [os.path.join(Q, "tools")]
sys.modules["tools"] = _tools
_tools._helpers = types.ModuleType("tools.tool_backend_helpers")
_tools._helpers.selection_error = lambda *a, **k: ""
_tools._helpers.selection_exists = lambda *a, **k: False
sys.modules["tools.tool_backend_helpers"] = _tools._helpers
_url = types.ModuleType("tools.url_safety")
_url.normalize_url_for_request = lambda u: u
sys.modules["tools.url_safety"] = _url
_res = types.ModuleType("tools.web_tools_rescue")
_res._rescue_eligible = lambda p: False
_res._rescue_extract = lambda *a, **k: []
sys.modules["tools.web_tools_rescue"] = _res

NAME = "w10_webtoolse_7dbad1_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "tools", "web_tools_extract.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "web_extract_url":
        return live._web_extract_url(r.get("value"))
    if f == "disabled_plugin_error":
        return live._disabled_plugin_error(r.get("capability", ""), r.get("disabled_key", ""))
    if f == "result_entry":
        return live._result_entry(r.get("url", ""), r.get("error"))
    if f == "extract_error_json":
        return live._extract_error_json(r.get("error", ""))
    if f == "merge_in_order":
        fixed = {int(k): v for k, v in (r.get("fixed") or {}).items()}
        return live._merge_in_order(r.get("total", 0), fixed, r.get("fetch_positions", []),
                                    r.get("fetch_urls", []), r.get("results", []))
    return None

n = m = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if not isinstance(r, dict) or "out" not in r:
        continue
    n += 1
    py, got = compute(r), r.get("out")
    if py != got:
        m += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w10_webtoolse_7dbad1: %d cases, %d mismatches" % (n, m))
sys.exit(1 if m else 0)
