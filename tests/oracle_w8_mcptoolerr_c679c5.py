#!/usr/bin/env python3
"""Differential oracle for tools/mcp_tool_errors.py (gate w8_mcptoolerr_c679c5)."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
SRC = Q + "/tools/mcp_tool_errors.py"
spec = importlib.util.spec_from_file_location("live_errors_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["live_errors_live"] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    nm = r.get("name", "s")
    if f == "resolve_identity_header":
        v = live._resolve_identity_header(nm, r.get("cfg", {}))
        return list(v) if v else None
    if f == "apply_identity_header":
        return live._apply_identity_header(nm, r.get("cfg", {}), dict(r.get("headers", {})))
    if f == "validate_remote_mcp_url":
        try:
            return {"ok": live._validate_remote_mcp_url(nm, r.get("url"))}
        except Exception as exc:
            return {"error": str(exc)}
    if f == "shown_url":
        return live._shown_url(r.get("url", ""))
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
print("ORACLE w8_mcptoolerr_c679c5: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
