#!/usr/bin/env python3
"""Oracle for the web_git FLOW port (src/cli/web_git_flow.c, wg_ prefix).

Drives the LIVE hermes_cli/web_git.py UNCLAIMED git-operations functions with
SCRIPTED I/O: the CLAIMED sibling helpers (_git, _git_write, _git_out, _git_ok,
_gh, _walk_entries, _untracked_insertions, _sanitize_branch), Path.is_dir/
is_file/resolve and the in-function import tracking_refspec are recording
stubs fed the same scripts the C harness feeds its universal seam. Compares
the DECISION SEQUENCE (ordered seam calls {"name","args"} in "trace") plus the
final return ("ret").

The live defs are AST-extracted (module constants seeded, Assign AND AnnAssign,
plain-Name targets only; the module has no classes). The heavy quarry import
chain is never imported. Fail-closed: missing defs or 0 cases => exit 2.
Summary: 'ORACLE web_git_flow: N cases, M mismatches'.

Representation contract (shared with the C port header):
- Tuples project to lists; None == JSON null; git/git_write/gh seams return
  [a, b, c]; walk_entries returns [[tag, xy, path], ...].
- Raise specs {"__raise__": kind, "message": str(exc)}; message is the EXACT
  str(exc). The BaseException trio uses the REAL builtins so scripted
  SystemExit/KeyboardInterrupt/GeneratorExit propagate through every live
  `except OSError` / `except RuntimeError` / `except JSONDecodeError` site.
- NUL-record convention: libjson strings are NUL-terminated, so fixture
  strings encode NUL as \\x01; the oracle decodes \\x01 -> \\x00 on inputs
  and encodes \\x00 -> \\x01 on outputs before comparing.
- Paths reach seams via Path.as_posix(); Path equality folds case on Windows
  (the C core folds ASCII case under _WIN32). os.devnull is the real host
  value ("nul" on Windows; the C core uses "nul" under _WIN32).
- int()/str()/json.dumps are ASCII-domain; str.strip() is full Unicode
  White_Space on both sides. Integral floats normalize to ints (canon).
"""
import ast
import json
import math
import os
import re
import sys
import types
import itertools
import shutil
import subprocess
import copy
from collections.abc import Callable
from pathlib import Path
from typing import Any, Optional

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "hermes_cli", "web_git.py")

TARGETS = [
    "_git_line", "_is_dir", "_status_z", "_origin_head", "_origin_head_abbrev",
    "_ref_exists", "_entry_staged", "_classify", "_status_letter",
    "_review_result", "_all_add_diff", "_single_file_diff", "file_diff_vs_head",
    "review_stage", "review_unstage", "review_revert", "review_rev_parse",
    "_has_staged", "_review_push", "review_push", "_gh_json", "review_ship_info",
    "_pr_query", "_pr_payload", "_own_pr", "review_create_pr", "worktree_list",
    "_main_root", "_fetch_tracking_ref", "_slugify", "_default_branch",
    "_ensure_repo", "_remote_of_ref", "worktree_remove", "_ref_names",
    "branch_list", "branch_switch", "base_branch_list",
]
# CLAIMED siblings seamed here (PoP grep of src/): _git/_git_write/_git_out/
# _git_ok (port_web_git_flow.c), _gh (env_passthrough.c), _walk_entries/
# _untracked_insertions/_sanitize_branch (web_git_branch.c).
SIBLINGS = [
    "_git", "_git_write", "_git_out", "_git_ok", "_gh",
    "_walk_entries", "_untracked_insertions", "_sanitize_branch",
]
CONSTANTS = [
    "_GIT_TIMEOUT", "_GH_TIMEOUT", "_GH_ERR_TAIL_CHARS",
    "_UNTRACKED_LINE_MAX_BYTES", "_UNTRACKED_SCAN_CAP",
    "_COMMIT_CONTEXT_DIFF_MAX_CHARS", "_COMMIT_CONTEXT_UNTRACKED_MAX",
    "_TRUNK_BRANCHES", "_GitRunner", "_BRANCH_SANITIZERS",
    "_PR_QUERY_BRANCH_CHUNK", "_PR_QUERY_BRANCH_CAP", "_PR_NODE_FIELDS",
]

import __future__ as _fut
_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    import textwrap
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def extract():
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("web_git_flow_live")
    sys.modules["web_git_flow_live"] = mod
    ns = mod.__dict__
    ns.update({
        "__name__": "web_git_flow_live",
        "re": re, "json": json, "os": os, "sys": sys, "types": types,
        "itertools": itertools, "shutil": shutil, "subprocess": subprocess,
        "Callable": Callable, "Path": Path, "Any": Any, "Optional": Optional,
        # module-level imports of _subprocess_compat (never called by the
        # unclaimed targets; the claimed siblings that use them are seamed).
        "FILTER_DISCOVERY_FAILED": "git filter discovery failed",
        "harden_git_argv": lambda args: list(args),
        "noninteractive_git_env": lambda *a, **k: {},
        "noninteractive_repo_git_env": lambda *a, **k: {},
    })
    tree = ast.parse(src)

    def seed_constants():
        for node in tree.body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
                if not all(isinstance(t, ast.Name) for t in tgts):
                    continue
                try:
                    exec_node(src, node, ns, SRC)
                except Exception:
                    pass

    seed_constants()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            try:
                exec_node(src, node, ns, SRC)
            except Exception as exc:
                print(f"FATAL oracle_web_git_flow: def {node.name}: {exc}")
                sys.exit(2)
    seed_constants()
    missing = [t for t in TARGETS + SIBLINGS + CONSTANTS if t not in ns]
    if missing:
        print(f"FATAL oracle_web_git_flow: missing defs: {missing}")
        sys.exit(2)
    return ns


NS = extract()
REAL = {nm: NS[nm] for nm in TARGETS + SIBLINGS}
REAL["_git"]._wg_tag = "read"
REAL["_git_write"]._wg_tag = "write"


# -- exception materialization (REAL builtins; BaseException trio passes) --
EXC_CLASSES = {
    "OSError": OSError, "PermissionError": PermissionError,
    "FileNotFoundError": FileNotFoundError, "FileExistsError": FileExistsError,
    "NotADirectoryError": NotADirectoryError, "IsADirectoryError": IsADirectoryError,
    "BlockingIOError": BlockingIOError, "InterruptedError": InterruptedError,
    "ProcessLookupError": ProcessLookupError, "TimeoutError": TimeoutError,
    "WindowsError": OSError,
    "SubprocessError": subprocess.SubprocessError,
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "IndexError": IndexError,
    "JSONDecodeError": json.JSONDecodeError, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    if cls is json.JSONDecodeError:
        return cls(msg, "", 0)
    return cls(msg)


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# -- NUL sentinel codec (libjson strings are NUL-terminated) ---------------
def nul_dec(v):
    if isinstance(v, str):
        return v.replace("\x01", "\x00")
    if isinstance(v, dict):
        return {k: nul_dec(x) for k, x in v.items()}
    if isinstance(v, list):
        return [nul_dec(x) for x in v]
    return v


def nul_enc(v):
    if isinstance(v, str):
        return v.replace("\x00", "\x01")
    if isinstance(v, dict):
        return {k: nul_enc(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [nul_enc(x) for x in v]
    return v


# -- per-case script backend -----------------------------------------------
class Ctx:
    def __init__(self, inp):
        self.io = inp.get("io") or {}
        self.cursors = {}
        self.trace = []

    def next_entry(self, name):
        if name not in self.io:
            raise RuntimeError(f"no script for {name}")
        entry = self.io[name]
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                raise RuntimeError(f"script exhausted for {name}")
            self.cursors[name] = idx + 1
            return entry[idx]
        return entry

    def call(self, name, args):
        self.trace.append({"name": name, "args": args})
        entry = self.next_entry(name)
        if isinstance(entry, dict) and entry.get("__script__"):
            if isinstance(entry.get("raise"), dict):
                raise make_exc(entry["raise"])
            return entry.get("ret")
        return entry


def install_case(ctx, inp):
    for nm, fn in REAL.items():
        NS[nm] = fn

    def git_rec(cwd, args, *, timeout=NS["_GIT_TIMEOUT"]):
        return ctx.call("git", {"cwd": cwd, "args": list(args), "timeout": timeout})

    def git_write_rec(cwd, args, *, timeout=NS["_GIT_TIMEOUT"]):
        return ctx.call("git_write", {"cwd": cwd, "args": list(args), "timeout": timeout})

    def git_out_rec(cwd, args, *, run=None):
        tag = getattr(run, "_wg_tag", "read")
        return ctx.call("git_out", {"cwd": cwd, "args": list(args), "run": tag})

    def git_ok_rec(cwd, args):
        return ctx.call("git_ok", {"cwd": cwd, "args": list(args)})

    def gh_rec(cwd, args):
        return ctx.call("gh", {"cwd": cwd, "args": list(args)})

    def walk_rec(raw):
        return ctx.call("walk_entries", {"raw": raw})

    def untracked_rec(cwd, rel):
        return ctx.call("untracked_insertions", {"cwd": cwd, "rel": rel})

    def sanitize_rec(name):
        return ctx.call("sanitize_branch", {"name": name})

    git_rec._wg_tag = "read"
    git_write_rec._wg_tag = "write"
    NS["_git"] = git_rec
    NS["_git_write"] = git_write_rec
    NS["_git_out"] = git_out_rec
    NS["_git_ok"] = git_ok_rec
    NS["_gh"] = gh_rec
    NS["_walk_entries"] = walk_rec
    NS["_untracked_insertions"] = untracked_rec
    NS["_sanitize_branch"] = sanitize_rec

    saved = (Path.is_dir, Path.is_file, Path.resolve)

    def is_dir_stub(self, **kw):
        return ctx.call("path.is_dir", {"path": self.as_posix()})

    def is_file_stub(self, **kw):
        return ctx.call("path.is_file", {"path": self.as_posix()})

    def resolve_stub(self, **kw):
        return Path(ctx.call("path.resolve", {"path": self.as_posix()}))

    Path.is_dir = is_dir_stub
    Path.is_file = is_file_stub
    Path.resolve = resolve_stub

    pkg = sys.modules.get("hermes_cli")
    if pkg is None:
        pkg = types.ModuleType("hermes_cli")
        pkg.__path__ = []
        sys.modules["hermes_cli"] = pkg
    ucc = types.ModuleType("hermes_cli.update_cmd_check")
    ucc.tracking_refspec = lambda remote, branch: ctx.call(
        "tracking_refspec", {"remote": remote, "branch": branch})
    sys.modules["hermes_cli.update_cmd_check"] = ucc
    pkg.update_cmd_check = ucc
    return saved


def restore_path(saved):
    Path.is_dir, Path.is_file, Path.resolve = saved


def _run(inp):
    return NS["_git_write"] if inp.get("run") == "write" else NS["_git"]


def run_case(func, inp):
    ctx = Ctx(inp)
    saved = install_case(ctx, inp)
    ret = None
    try:
        if func == "_git_line":
            ret = NS["_git_line"](inp["cwd"], inp["args"], run=_run(inp))
        elif func == "_is_dir":
            ret = NS["_is_dir"](inp["cwd"])
        elif func == "_status_z":
            ret = NS["_status_z"](inp["cwd"], *(inp.get("extra") or []), run=_run(inp))
        elif func == "_origin_head":
            ret = NS["_origin_head"](inp["cwd"], run=_run(inp))
        elif func == "_origin_head_abbrev":
            ret = NS["_origin_head_abbrev"](inp["cwd"])
        elif func == "_ref_exists":
            ret = NS["_ref_exists"](inp["cwd"], inp["ref"], run=_run(inp))
        elif func == "_entry_staged":
            ret = NS["_entry_staged"](inp["tag"], inp["xy"])
        elif func == "_classify":
            ret = NS["_classify"](inp["tag"], inp["xy"], inp["path"])
        elif func == "_status_letter":
            ret = NS["_status_letter"](inp["tag"], inp["xy"])
        elif func == "_review_result":
            ret = NS["_review_result"](inp["cwd"], copy.deepcopy(inp.get("files") or []), inp.get("base"))
        elif func == "_all_add_diff":
            ret = NS["_all_add_diff"](inp["cwd"], inp["file_path"])
        elif func == "_single_file_diff":
            ret = NS["_single_file_diff"](inp["cwd"], inp["args"], inp["file_path"])
        elif func == "file_diff_vs_head":
            ret = NS["file_diff_vs_head"](inp["cwd"], inp["file_path"])
        elif func == "review_stage":
            ret = NS["review_stage"](inp["cwd"], inp.get("file_path"))
        elif func == "review_unstage":
            ret = NS["review_unstage"](inp["cwd"], inp.get("file_path"))
        elif func == "review_revert":
            ret = NS["review_revert"](inp["cwd"], inp.get("file_path"))
        elif func == "review_rev_parse":
            ret = NS["review_rev_parse"](inp["cwd"], inp.get("ref"))
        elif func == "_has_staged":
            ret = NS["_has_staged"](inp["raw"])
        elif func == "_review_push":
            ret = NS["_review_push"](inp["cwd"])
        elif func == "review_push":
            ret = NS["review_push"](inp["cwd"])
        elif func == "_gh_json":
            ret = NS["_gh_json"](inp["cwd"], inp["args"])
        elif func == "review_ship_info":
            ret = NS["review_ship_info"](inp["cwd"])
        elif func == "_pr_query":
            ret = NS["_pr_query"](inp["owner"], inp["name"], inp["branches"], inp["numbers"])
        elif func == "_pr_payload":
            ret = NS["_pr_payload"](inp["pr"])
        elif func == "_own_pr":
            ret = NS["_own_pr"](inp["key"], inp["field"])
        elif func == "review_create_pr":
            ret = NS["review_create_pr"](inp["cwd"])
        elif func == "worktree_list":
            ret = NS["worktree_list"](inp["cwd"], run=_run(inp))
        elif func == "_main_root":
            ret = NS["_main_root"](inp["cwd"])
        elif func == "_fetch_tracking_ref":
            ret = NS["_fetch_tracking_ref"](inp["root"], inp["remote"], inp["branch"])
        elif func == "_slugify":
            ret = NS["_slugify"](inp.get("name"))
        elif func == "_default_branch":
            ret = NS["_default_branch"](inp["cwd"], run=_run(inp))
        elif func == "_ensure_repo":
            ret = NS["_ensure_repo"](inp["cwd"])
        elif func == "_remote_of_ref":
            ret = NS["_remote_of_ref"](inp["cwd"], inp["name"])
        elif func == "worktree_remove":
            ret = NS["worktree_remove"](inp["cwd"], inp["worktree_path"], inp.get("force", False))
        elif func == "_ref_names":
            ret = NS["_ref_names"](inp["cwd"], *(inp.get("patterns") or []),
                                   fmt=inp.get("fmt", "%(refname:short)"))
        elif func == "branch_list":
            ret = NS["branch_list"](inp["cwd"])
        elif func == "branch_switch":
            ret = NS["branch_switch"](inp["cwd"], inp["branch"])
        elif func == "base_branch_list":
            ret = NS["base_branch_list"](inp["cwd"])
        else:
            return {"error": "unknown func"}
    except BaseException as exc:  # noqa: BLE001 - raise sentinel parity (SystemExit!)
        ret = proj_exc(exc)
    finally:
        restore_path(saved)
    return {"ret": ret, "trace": ctx.trace}


def canon(v):
    if isinstance(v, float):
        if math.isnan(v):
            return {"__float__": "nan"}
        if math.isinf(v):
            return {"__float__": "-inf" if v < 0 else "inf"}
        if v == int(v) and abs(v) < 1e16:
            return int(v)
        return v
    if isinstance(v, dict):
        return {k: canon(x) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [canon(x) for x in v]
    return v


def deep_eq(a, b):
    return canon(a) == canon(b)


def main():
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line or not line.startswith("{"):
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = rec.get("func")
        inp = rec.get("in", {}) or {}
        c_out = rec.get("out")
        cases += 1
        try:
            py_out = run_case(func, nul_dec(inp))
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print(f"PYERR {func}: {type(exc).__name__}: {exc}")
            continue
        if c_out is None:
            mism += 1
            print(f"MISMATCH {func}: missing C out")
            continue
        want = nul_enc(py_out)
        bad = []
        for key, wv in want.items():
            gv = c_out.get(key)
            if not deep_eq(gv, wv):
                bad.append(key)
        if bad:
            mism += 1
            print(f"MISMATCH {func} keys={bad}")
            for key in bad:
                print(f"  C.{key}={json.dumps(canon(c_out.get(key)), default=str)[:600]}")
                print(f"  PY.{key}={json.dumps(canon(want.get(key)), default=str)[:600]}")
    if cases == 0:
        print("FATAL oracle_web_git_flow: 0 cases")
        print("ORACLE web_git_flow: 0 cases, 0 mismatches")
        return 2
    print(f"ORACLE web_git_flow: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())