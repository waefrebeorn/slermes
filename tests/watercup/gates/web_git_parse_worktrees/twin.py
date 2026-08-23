"""twin for web_git_parse_worktrees.

Imports LIVE hermes_cli/web_git.py::_parse_worktrees and traces the SAME
per-line rung ladder as the C harness over the same porcelain blobs.
Result is serialized to a canonical string identical to the C harness's
(path|branch|detached,bare,locked,isMain; per tree, in order).
"""
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_web_git", os.path.join(DEV, "hermes_cli", "web_git.py"))
_wg = ilu.module_from_spec(_spec)
sys.modules["wc_live_web_git"] = _wg
try:
    _spec.loader.exec_module(_wg)
except Exception as e:
    raise RuntimeError("cannot import LIVE web_git: %s" % e)


def canon(trees):
    parts = []
    for i, t in enumerate(trees):
        parts.append("%s|%s|%d%d%d%d;" % (
            t.get("path") or "",
            t.get("branch") or "",
            1 if t.get("detached") else 0,
            1 if t.get("bare") else 0,
            1 if t.get("locked") else 0,
            1 if i == 0 else 0))
    return "".join(parts)


def trace(blob):
    c = Cup("parse_worktrees")
    cur = None
    for line in blob.split("\n"):
        b1 = line.startswith("worktree ")
        c.branch(b1)
        if b1:
            c.branch(cur is not None)
            cur = {"path": line[9:].strip(), "branch": None,
                   "detached": False, "bare": False, "locked": False}
        elif cur is None:
            continue
        else:
            b3 = line.startswith("branch ")
            c.branch(b3)
            if b3:
                cur["branch"] = line[7:].strip().replace("refs/heads/", "", 1)
            else:
                c.branch(line == "detached")
                if line == "detached":
                    cur["detached"] = True
                else:
                    c.branch(line == "bare")
                    if line == "bare":
                        cur["bare"] = True
                    else:
                        c.branch(line.startswith("locked"))
                        if line.startswith("locked"):
                            cur["locked"] = True
    c.branch(cur is not None)
    trees = []
    if cur is not None:
        trees.append(cur)
    # LIVE _parse_worktrees returns list-of-dicts; use it to arbitrate values
    live_trees = _wg._parse_worktrees(blob)
    c.valstr(canon(live_trees))
    return c.finalize()


def run(inputs):
    return [trace(b) for b in inputs]
