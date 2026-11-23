#!/usr/bin/env python3
"""Differential oracle for tools/mcp_liveness.py (gate w6_mcpliveness_08bec8)."""
import importlib.util
import json
import sys
from types import SimpleNamespace

QUARRY = "C:/Users/eman5/hermes-agent"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)

SRC = QUARRY + "/tools/mcp_liveness.py"
spec = importlib.util.spec_from_file_location("w6_mcpliveness_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "action":
        return list(live._action(r.get("state", ""), r.get("app_name", ""), r.get("gpu_label", "")))
    if func == "availability_problem":
        return live.availability_problem(SimpleNamespace(state=r.get("state", "")))
    if func == "parse":
        f = r.get("facts", {})
        live.facts.os_family = lambda: f.get("osf", "win32")
        try:
            lv = live.parse_liveness(f.get("raw"))
            return {"ok": True, "kind": lv.kind, "path": lv.path,
                    "url_field": lv.url_field, "token_field": lv.token_field,
                    "pid_field": lv.pid_field}
        except ValueError as exc:
            return {"ok": False, "error": str(exc)}
    if func == "describe":
        f = r.get("facts", {})
        decl = SimpleNamespace(name=f.get("name", ""), required_gpu=f.get("required_gpu"))
        av = f.get("available", {})
        available = SimpleNamespace(state=av.get("state", ""),
                                    min_version=av.get("min_version"),
                                    version=av.get("version"))
        return live.describe(decl, available, f.get("liveness_state", ""), f.get("display_name"))
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s %r" % (r.get("func"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)

print("ORACLE w6_mcpliveness_08bec8: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
