#!/usr/bin/env python3
"""Oracle for gate w17_worktreere_5152ed (hermes_cli/worktree_retention.py)."""
import importlib.util
import json
import sys
import types
import contextlib
import io

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

def _load(dotted, rel):
    parts = dotted.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pkg = types.ModuleType(pn)
            pkg.__path__ = [QUARRY + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pkg
    spec = importlib.util.spec_from_file_location(dotted, QUARRY + "/" + rel)
    m = importlib.util.module_from_spec(spec)
    sys.modules[dotted] = m
    with contextlib.redirect_stderr(io.StringIO()):
        spec.loader.exec_module(m)
    return m

live = _load("hermes_cli.worktree_retention", "hermes_cli/worktree_retention.py")

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
    if func == "named":
        paths = r.get("paths") or []
        py = live._named(paths, bool(r.get("more", False)))
    elif func == "constants":
        py = {
            "KEEP_UNARCHIVABLE": live.KEEP_UNARCHIVABLE,
            "KEEP_WORKTREE_REFS": live.KEEP_WORKTREE_REFS,
            "KEEP_LARGE_UNTRACKED": live.KEEP_LARGE_UNTRACKED,
            "KEEP_SUBMODULE_WORK": live.KEEP_SUBMODULE_WORK,
            "_NAMED": live._NAMED,
            "_MAX_ENTRIES": live._MAX_ENTRIES,
        }
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w17_worktreere_5152ed: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
