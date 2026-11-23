#!/usr/bin/env python3
"""Differential oracle for tools/mcp_tool_config.py (gate w8_mcptoolcon_809435)."""
import importlib.util, json, os, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
SRC = Q + "/tools/mcp_tool_config.py"
spec = importlib.util.spec_from_file_location("live_config_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["live_config_live"] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "pathext_suffixes":
        saved = live.os.environ
        live.os.environ = {}
        try:
            return live._pathext_suffixes(r.get("env", {}), windows=r.get("windows", True))
        finally:
            live.os.environ = saved
    if f == "warn_hidden_whitespace":
        return live._warn_hidden_whitespace(r.get("name", ""), r.get("config", {}))
    if f == "require_rendered_remote":
        try:
            return {"ok": live._require_rendered_remote(r.get("name", ""), r.get("cfg", {}))}
        except Exception as exc:
            return {"error": str(exc)}
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
print("ORACLE w8_mcptoolcon_809435: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
