#!/usr/bin/env python3
"""Differential oracle for hermes_cli/browser_runtime.py (gate w13_browserrun_9220bd).

The live module's four source lookups (config override / pm build / baked path /
system command) and the termux/baked-exists predicates are stubbed from each
case; the LIVE chromium_executable / chromium_missing_hint functions run.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
SRC = "C:/Users/eman5/hermes-agent/hermes_cli/browser_runtime.py"
spec = importlib.util.spec_from_file_location("w13browser_live", SRC)
live = importlib.util.module_from_spec(spec)
spec.loader.exec_module(live)

# Parent-package bootstrap for the late `from hermes_platform.host.runtime import is_termux`.
hp = types.ModuleType("hermes_platform"); hp.__path__ = []
host = types.ModuleType("hermes_platform.host"); host.__path__ = []
runtime = types.ModuleType("hermes_platform.host.runtime")
sys.modules["hermes_platform"] = hp
sys.modules["hermes_platform.host"] = host
sys.modules["hermes_platform.host.runtime"] = runtime


def stubs(r):
    configured = r.get("configured") or r.get("pinned") or None
    pm_exec = r.get("pm_exec") or None
    baked = r.get("baked") or None
    system = r.get("system") or None
    pm_no = bool(r.get("pm_no", False))
    live._configured_executable = lambda: configured
    live._pm_executable = lambda: pm_exec
    live._baked_executable = lambda: baked
    live._system_executable = lambda: system
    live._pm_has_no_chromium = lambda: pm_no
    live.BAKED_EXECUTABLE_FILE = types.SimpleNamespace(
        exists=lambda: bool(r.get("baked_exists", False)))
    runtime.is_termux = lambda: bool(r.get("termux", False))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    stubs(r)
    if func == "chromium_executable":
        want = live.chromium_executable(allow_override=bool(r.get("allow_override", True)))
    elif func == "chromium_missing_hint":
        want = live.chromium_missing_hint()
    else:
        continue
    got = r.get("out")
    cases += 1
    if want != got:
        mism += 1
        print(f"MISMATCH {func} in={r} py={want!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w13_browserrun_9220bd: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
