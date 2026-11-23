#!/usr/bin/env python3
"""Differential oracle for upstream Hermes hermes_cli/main_resume_args.py (gate w15_mainresume_0c8e53).

Each CLI step's dependency import is replaced by a sentinel-raising fake, so
calling the live function reveals its guard decision (and the normalized
foreign-resume kind / worktree resume it would act on) without chdir, DB or
session-import side effects. The C port implements exactly those guards.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


SRC = QUARRY + "/hermes_cli/main_resume_args.py"
spec = importlib.util.spec_from_file_location("w15_mainresume_0c8e53_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w15_mainresume_0c8e53_live"] = live
spec.loader.exec_module(live)


class _Sentinel(Exception):
    pass


class _Args:
    pass


def compute(r):
    func = r.get("func")

    if func == "in_dir_guard":
        tools = _mod("tools"); tools.__path__ = []
        env = _mod("tools.environments"); env.__path__ = []

        def _conv(_v):
            raise _Sentinel()

        _mod("tools.environments.local", _msys_to_windows_path=_conv)
        a = _Args(); a.in_dir = r.get("in_dir")
        try:
            live.apply_in_dir(a)
            return False
        except _Sentinel:
            return True

    if func == "foreign_guard":
        seen = {}

        def _pick(kind):
            seen["kind"] = kind
            raise _Sentinel()

        _mod("hermes_cli.foreign_sessions",
             pick_foreign_session=_pick,
             import_foreign_session=lambda *a, **k: None)
        a = _Args(); a.resume = r.get("resume")
        try:
            live.import_foreign_resume(a)
            return {"acts": False, "kind": None}
        except _Sentinel:
            return {"acts": True, "kind": seen.get("kind")}

    if func == "worktree_guard":
        seen = {}

        def _restore(x):
            seen["resume"] = x
            raise _Sentinel()

        _mod("hermes_cli.session_worktree", restore=_restore)
        a = _Args(); a.resume = r.get("resume"); a.worktree = r.get("worktree", False)
        try:
            live.recreate_resumed_tui_worktree(a)
            return {"acts": False, "resume": None}
        except _Sentinel:
            return {"acts": True, "resume": seen.get("resume")}

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
        print(f"ORACLE w15_mainresume_0c8e53: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w15_mainresume_0c8e53: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
