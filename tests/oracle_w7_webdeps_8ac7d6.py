#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_deps.py (gate w7_webdeps_8ac7d6)."""
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

# Load the live module and register a per-case fake web_server module so the
# late-bound _server(module) resolution finds a scripted target.
spec = importlib.util.spec_from_file_location(
    "w7_webdeps_live", QUARRY + "/hermes_cli/web_deps.py")
live = importlib.util.module_from_spec(spec)
sys.modules["w7_webdeps_live"] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "late_name":
        proxy = live.late(r.get("name") or "", r.get("module") or live.WEB_SERVER)
        return [proxy.__name__, proxy.__qualname__]
    if func == "state":
        module = r.get("module") or live.WEB_SERVER
        name = r.get("name") or "state"
        target = r.get("target")
        if isinstance(target, dict) and r.get("op") == "attr":
            target = types.SimpleNamespace(**target)
        srv = types.ModuleType(module)
        setattr(srv, name, target)
        sys.modules[module] = srv
        st = live.LateState(name, module)
        op, arg = r.get("op"), r.get("arg")
        if op == "getitem":   return st[arg]
        if op == "contains":  return arg in st
        if op == "len":       return len(st)
        if op == "bool":      return bool(st)
        if op == "repr":      return repr(st)
        if op == "attr":      return getattr(st, arg)
        if op == "eq":        return st == arg
        if op == "ne":        return st != arg
        if op == "lt":        return st < arg
        if op == "le":        return st <= arg
        if op == "gt":        return st > arg
        if op == "ge":        return st >= arg
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
    try:
        py = compute(r)
    except Exception as exc:
        print("ORACLE w7_webdeps_8ac7d6: live raised %r case %r" % (exc, r.get("case")),
              file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    cases += 1
    if py != got:
        mism += 1
        print("MISMATCH w7_webdeps_8ac7d6 case=%s func=%s op=%s py=%r c=%r" %
              (r.get("case"), r.get("func"), r.get("op"), py, got), file=sys.stderr)

print("ORACLE w7_webdeps_8ac7d6: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
