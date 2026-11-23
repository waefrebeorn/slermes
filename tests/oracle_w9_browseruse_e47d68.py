#!/usr/bin/env python3
"""Differential oracle for tools/browser_use_shot.py (gate w9_browseruse_e47d68)."""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m

bh = _mod("browser_harness"); bh.__path__ = [Q + "/browser_harness"]
_mod("browser_harness.admin", daemon_browser_kind=lambda: _KIND[0])
_KIND = ["local"]

NAME = "w9_browseruse_e47d68_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/tools/browser_use_shot.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "harness_command":
        tf = r.get("this_file", "")
        old = live.os.path.abspath
        live.os.path.abspath = lambda p: p
        try:
            live.__file__ = tf
            return live.harness_command(r.get("cmd", []), r.get("activate", "never"))
        finally:
            live.os.path.abspath = old
    if f == "activation":
        _KIND[0] = "cloud" if r.get("is_cloud") else "local"
        return bool(live._activation_check(r.get("mode", "never"))())
    if f == "is_harness_helper":
        if r.get("callable"):
            fn = lambda: None
            fn.__module__ = r.get("module", "")
        else:
            fn = r.get("module", "")
        return live._is_harness_helper(fn)
    if f == "constants":
        return {"png_deadline_s": live.PNG_DEADLINE_S,
                 "jpeg_deadline_s": live.JPEG_DEADLINE_S,
                 "harness_module": live._HARNESS_MODULE,
                 "activate_never": live.ACTIVATE_NEVER,
                 "activate_always": live.ACTIVATE_ALWAYS,
                 "activate_if_cloud": live.ACTIVATE_IF_CLOUD,
                 "harness_module_args": list(live._HARNESS_MODULE_ARGS)}
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
        print(f"ORACLE w9_browseruse_e47d68: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w9_browseruse_e47d68 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w9_browseruse_e47d68: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
