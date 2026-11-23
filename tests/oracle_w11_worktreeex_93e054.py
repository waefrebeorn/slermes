#!/usr/bin/env python3
"""Differential oracle for gate w11_worktreeex_93e054 (hermes_cli/worktree_exit.py)."""
import importlib.util, json, os, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
for _pkg in ("hermes_cli", "agent"):
    if _pkg not in sys.modules:
        _m = types.ModuleType(_pkg)
        _m.__path__ = [os.path.join(Q, _pkg)]
        sys.modules[_pkg] = _m
NAME = "w11_worktreeex_93e054_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "hermes_cli", "worktree_exit.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def _patch_ops(dirty, unpushed):
    wo = types.ModuleType("hermes_cli.worktree_ops")
    wo._worktree_is_dirty = lambda p, rr, timeout=10: dirty
    wo._worktree_has_unpushed_commits = lambda p, timeout=10: unpushed
    sys.modules["hermes_cli.worktree_ops"] = wo
    sys.modules["hermes_cli"].worktree_ops = wo


def compute(r):
    f = r.get("func")
    info = r.get("info", None)
    if f == "worktree_state":
        isdir = bool(r.get("is_dir", False))
        _patch_ops(bool(r.get("is_dirty", False)), bool(r.get("has_unpushed", False)))
        orig = os.path.isdir
        os.path.isdir = lambda p: isdir
        try:
            return live.worktree_state(info)
        finally:
            os.path.isdir = orig
    if f == "ask_exit_choice":
        inf = json.loads(json.dumps(info)) if info is not None else None
        has_tty = bool(r.get("has_tty", False))
        state = r.get("state", None)
        in_use = bool(r.get("still_in_use", False))
        idx = r.get("curses_idx", None)
        err = bool(r.get("curses_error", False))
        live._has_tty = lambda: has_tty
        live.worktree_state = lambda i: state
        live._still_in_use = lambda p: in_use
        cu = types.ModuleType("hermes_cli.curses_ui")
        def _list(*a, **k):
            if err:
                raise RuntimeError("boom")
            return idx
        cu.curses_radiolist = _list
        sys.modules["hermes_cli.curses_ui"] = cu
        sys.modules["hermes_cli"].curses_ui = cu
        return live.ask_exit_choice(inf)
    return None


n = m = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if not isinstance(r, dict) or "out" not in r:
        continue
    n += 1
    py, got = compute(r), r.get("out")
    if py != got:
        m += 1
        print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
print("ORACLE w11_worktreeex_93e054: %d cases, %d mismatches" % (n, m))
sys.exit(1 if m else 0)
