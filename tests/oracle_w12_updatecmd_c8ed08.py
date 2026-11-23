#!/usr/bin/env python3
"""Differential oracle for hermes_cli/update_cmd_git.py (gate w12_updatecmd_c8ed08)."""
import importlib.util
import json
import os
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

if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m

_sc = types.ModuleType("hermes_cli._subprocess_compat")
_sc.NO_LAZY_FETCH_ENV = {}
_sc.noninteractive_git_env = lambda: {}
_sc.windows_hide_flags = lambda: 0
sys.modules["hermes_cli._subprocess_compat"] = _sc
_cc = types.ModuleType("hermes_cli.update_cmd_common")
_cc._record_stop = lambda *a, **k: None
sys.modules["hermes_cli.update_cmd_common"] = _cc

live = load("w12_updatecmd_c8ed08_live", QUARRY + "/hermes_cli/update_cmd_git.py")

def compute(r):
    f = r.get("func")
    if f == "is_fork":
        return live._is_fork(r.get("origin_url"))
    if f == "has_http_code":
        return live._has_http_code(r.get("stderr_text", ""), *r.get("codes", []))
    if f == "classify_fetch_failure":
        return live._classify_fetch_failure(r.get("stderr_text", ""))
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
    except Exception as exc:
        print("ORACLE w12_updatecmd_c8ed08: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w12_updatecmd_c8ed08 %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w12_updatecmd_c8ed08: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
