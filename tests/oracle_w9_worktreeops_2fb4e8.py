#!/usr/bin/env python3
"""Differential oracle for hermes_cli/worktree_ops.py (gate w9_worktreeops_2fb4e8).

The git subprocess layer is never reached: only the pure path/string decision
functions are called on the LIVE module.
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

if "hermes_cli" not in sys.modules:
    _pkg = types.ModuleType("hermes_cli")
    _pkg.__path__ = [QUARRY + "/hermes_cli"]
    sys.modules["hermes_cli"] = _pkg
_wsc = types.ModuleType("hermes_cli._subprocess_compat")
_wsc.noninteractive_git_env = lambda *a, **k: {}
sys.modules["hermes_cli._subprocess_compat"] = _wsc
_ggc = types.ModuleType("hermes_cli.git_commit_graph")
for name in ("claim_commit_graph_refresh", "claim_maintenance_slot", "common_git_dir",
             "refresh_commit_graph", "run_bounded_git"):
    setattr(_ggc, name, lambda *a, **k: None)
sys.modules["hermes_cli.git_commit_graph"] = _ggc
_hc = types.ModuleType("hermes_constants")
_hc.get_hermes_home = lambda: ""
sys.modules["hermes_constants"] = _hc
_ut = types.ModuleType("utils")
_ut.atomic_json_write = lambda *a, **k: None
sys.modules["utils"] = _ut


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


live = load("w9_worktreeops_2fb4e8_live", QUARRY + "/hermes_cli/worktree_ops.py")


def compute(r):
    f = r.get("func")
    if f == "normalize_git_bash_path":
        return live._normalize_git_bash_path(r.get("p", ""))
    if f == "prune_tier_hours":
        return live.prune_tier_hours(r.get("name", ""), float(r.get("max_age_hours", 0)))
    if f == "path_is_within_root":
        from pathlib import Path
        return live._path_is_within_root(Path(r.get("path", "")), Path(r.get("root", "")))
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
        print("ORACLE w9_worktreeops_2fb4e8: live raised %r case %s" % (exc, r.get("case")), file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH w9_worktreeops_2fb4e8 %s case=%s py=%r c=%r" %
              (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w9_worktreeops_2fb4e8: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
