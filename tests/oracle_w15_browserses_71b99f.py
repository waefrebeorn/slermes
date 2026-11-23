#!/usr/bin/env python3
"""Differential oracle for tools/browser_session_cloud.py (gate w15_browserses_71b99f)."""
import importlib.util
import json
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

NAME = "tools.browser_session_cloud_live"
spec = importlib.util.spec_from_file_location(NAME, QUARRY + "/tools/browser_session_cloud.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def compute(r):
    section = r.get("section") or {}
    live.browser_section = lambda: section
    live.reset_caches()                 # _memo caches across calls otherwise
    f = r.get("func")
    if f == "command_timeout":
        return live.command_timeout()
    if f == "inactivity_timeout":
        return live.inactivity_timeout(r.get("default", 0))
    if f == "chromium_args":
        return live.chromium_args()
    if f == "executable_path":
        return live.executable_path()
    if f == "browser_engine":
        return live.get_browser_engine()
    if f == "is_headed":
        return live.is_headed_mode()
    if f == "allow_private_urls":
        return live.allow_private_urls()
    if f == "auto_local_private":
        return live.auto_local_for_private_urls()
    if f == "use_real_profile":
        return live.use_real_profile()
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
        print(f"ORACLE w15_browserses_71b99f: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w15_browserses_71b99f {r.get('func')} case={r.get('case')} py={py!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE w15_browserses_71b99f: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
