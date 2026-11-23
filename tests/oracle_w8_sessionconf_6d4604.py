#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/session_config.py
(gate w8_sessionconf_6d4604).

mcp_revision is compared against the real function with a policy stand-in.
The parameter-validation prefixes of config_get/model_options and the _fast
tier mapping are compared against the exact source expressions.
"""
import importlib.util
import json
import sys
from types import SimpleNamespace

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

spec = importlib.util.spec_from_file_location("session_config_live", Q + "/gateway/session_config.py")
live = importlib.util.module_from_spec(spec)
sys.modules["session_config_live"] = live
spec.loader.exec_module(live)

def config_get_problem(p):
    if set(p) - {'session_id', 'profile', 'key', 'cwd'}:
        return 'invalid_params'
    key = p.get('key')
    if not isinstance(key, str):
        return 'invalid_params'
    if key in ('busy', 'verbose', 'yolo', 'show_reasoning'):
        return 'busy'
    if 'cwd' in p and (key != 'project' or not isinstance(p['cwd'], str)):
        return 'invalid_params'
    return 'ok'

def model_options_problem(p):
    flags = {'explicit_only', 'include_unconfigured', 'refresh'}
    if (set(p) - flags - {'profile', 'session_id'}
            or any(type(p[k]) is not bool for k in flags & p.keys())):
        return 'invalid_params'
    return 'ok'

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
        if func == "mcp_revision":
            src = r.get("source") or {}
            if r.get("has_policy"):
                py = live.mcp_revision(SimpleNamespace(config=lambda: src), {})
            else:
                py = live.mcp_revision(None, src)
        elif func == "config_get_problem":
            py = config_get_problem(r.get("params") or {})
        elif func == "model_options_problem":
            py = model_options_problem(r.get("params") or {})
        elif func == "fast_value":
            py = {'priority': 'fast', 'ultrafast': 'ultrafast'}.get(r.get("tier"), 'normal')
        else:
            continue
    except Exception as exc:
        print("LIVE ERROR %s %r: %r" % (func, r, exc), file=sys.stderr)
        mism += 1
        cases += 1
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w8_sessionconf_6d4604: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
