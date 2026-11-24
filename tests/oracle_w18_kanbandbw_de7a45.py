#!/usr/bin/env python3
"""Differential oracle for gate w18_kanbandbw_de7a45 (hermes_cli/kanban_db_workspace.py)."""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if not (Path(QUARRY) / "hermes_cli" / "kanban_db_workspace.py").is_file():
    QUARRY = str(Path(__file__).resolve().parent.parent.parent / "hermes-agent")
sys.path.insert(0, QUARRY)

_pkg = types.ModuleType("hermes_cli")
_pkg.__path__ = []
sys.modules["hermes_cli"] = _pkg
_wo = types.ModuleType("hermes_cli.worktree_ops")
_wo.release_lsp_clients = lambda *a, **k: None
sys.modules["hermes_cli.worktree_ops"] = _wo
_kb = types.ModuleType("hermes_cli.kanban_db")
_kb.__path__ = []
sys.modules["hermes_cli.kanban_db"] = _kb

_SPEC = importlib.util.spec_from_file_location(
    "w18_kanbandbw_live", str(Path(QUARRY) / "hermes_cli" / "kanban_db_workspace.py"))
mod = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = mod
_SPEC.loader.exec_module(mod)

CONST_NAMES = [
    "_TERMINAL_STATUSES_SQL", "_WORKSPACE_ROW_SQL", "_ACTIVE_CHILDREN_SQL",
    "_TERMINAL_TASK_SQL", "_OTHER_LIVE_PATHS_SQL", "_ANY_LIVE_PATHS_SQL",
    "_REMOVABLE_KINDS", "_SCRATCH_TIP_SENTINEL_NAME", "_SCRATCH_TIP_MESSAGE",
]


def _canon(x):
    if isinstance(x, (list, tuple)): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((str(k), _canon(v)) for k, v in x.items()))
    if isinstance(x, bool): return ("num", 1 if x else 0)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if x is None: return ("null",)
    return ("other", repr(x))


def expected(rec):
    f = rec.get("func")
    if f == "path_key":
        return mod._path_key(rec.get("path"))
    if f == "constants":
        return {name: getattr(mod, name) for name in CONST_NAMES}
    raise RuntimeError("unknown func " + str(f))


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        got = rec.get("out")
        want = expected(rec)
        if _canon(got) != _canon(want):
            mism += 1
            if mism <= 10:
                print("MISMATCH %s case=%s got=%r want=%r" %
                      (rec.get("func"), rec.get("case"), got, want), file=sys.stderr)
    print("ORACLE w18_kanbandbw_de7a45: %d cases, %d mismatches" % (n, mism))
    sys.exit(1 if (mism or n == 0) else 0)


if __name__ == "__main__":
    main()
