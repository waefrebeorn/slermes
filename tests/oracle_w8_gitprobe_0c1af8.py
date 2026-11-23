#!/usr/bin/env python3
"""Differential oracle for tui_gateway/git_probe.py (gate w8_gitprobe_0c1af8).

The two ported functions are the pure decision layers of the module; their git
spawns are injected as strings/roots by the harness.  Here the LIVE functions
are replayed with those seams monkeypatched, so no git subprocess runs.
"""
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

# Bootstrap the hermes_cli parent package so the real _subprocess_compat import
# in git_probe resolves from the quarry.
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


live = load("w8_gitprobe_0c1af8_live", QUARRY + "/tui_gateway/git_probe.py")


def compute(r):
    func = r.get("func")
    if func == "branch":
        first, second = r.get("first", ""), r.get("second", "")

        def fake_run_git(cwd, *args):
            if args and args[0] == "branch":
                return first
            if tuple(args[:2]) == ("rev-parse", "--short"):
                return second
            return ""

        old = live.run_git
        live.run_git = fake_run_git
        try:
            return live.branch("cwd")
        finally:
            live.run_git = old
    if func == "resolve":
        wt, cr = r.get("worktree_root") or "", r.get("common_root") or ""
        old_rr, old_crr = live.repo_root, live.common_repo_root
        live.repo_root = lambda c: wt
        live.common_repo_root = lambda c: cr
        try:
            return live.resolve("cwd")
        finally:
            live.repo_root, live.common_repo_root = old_rr, old_crr
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
        print("ORACLE w8_gitprobe_0c1af8: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w8_gitprobe_0c1af8 %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w8_gitprobe_0c1af8: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
