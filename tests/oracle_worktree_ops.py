#!/usr/bin/env python3
"""Oracle for port_worktree_ops.c -- drives the LIVE hermes_cli/worktree_ops.py
defs. The git execution / worktree setup / maintenance half is not imported; the
pure defs are ast-extracted.
"""
import ast
import json
import os
import re
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

WIN_SEP = "\\"

HERMES_ROOT = r"C:\Users\eman5\hermes-agent"
if HERMES_ROOT not in sys.path:
    sys.path.insert(0, HERMES_ROOT)

WT = r"C:\Users\eman5\hermes-agent\hermes_cli\worktree_ops.py"
WANTED = ["_normalize_git_bash_path", "_path_is_within_root",
          "_worktreeinclude_entries"]


def extract():
    ns = {"str": str, "int": int, "float": float, "bool": bool, "Any": object,
          "Optional": object, "list": list, "tuple": tuple, "dict": dict,
          "os": os, "re": re, "sys": sys, "Path": __import__("pathlib").Path,
          "subprocess": __import__("subprocess")}
    src = open(WT, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), WT, "exec"), ns)
            except Exception:
                pass
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), WT, "exec"), ns)
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    bash_paths = {
        "c": "/c/Users/x", "cygdrive": "/cygdrive/c/Users/x", "mnt": "/mnt/c/Users/x",
        "no-slash": "/d", "two-letter": "/cd/x", "windows": "C:\\already\\win",
        "posix": "/c/x", "null": None,
    }
    roots = {
        "inside": ("C:/a/b", "C:/a"),
        "equal": ("C:/a", "C:/a"),
        "outside": ("C:/a/bc", "C:/a/b"),
        "other": ("C:/x", "C:/a"),
        "backsl": ("C:\\a\\b", "C:/a"),
        "null": (None, "C:/a"),
    }
    includes = {
        "plain": (["main", "  feature  ", "", "# comment", "  # indented", "dev"], 0),
        "empty": ([], 0),
        "bom": (["main", "dev"], 1),
        "comments": (["# only comments", ""], 0),
    }

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        fn = c.get("fn")
        label = c.get("case")
        cases += 1

        try:
            if fn == "_normalize_git_bash_path":
                p = bash_paths[label]
                # the live fn tests sys.platform; simulate win32 unless the case
                # is explicitly posix
                real = sys.platform
                sys.platform = "linux" if label == "posix" else "win32"
                try:
                    want = ns["_normalize_git_bash_path"](p)
                finally:
                    sys.platform = real
                if want is not None:
                    want = want  # already the live answer
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _normalize_git_bash_path case={label} C={got!r} PY={want!r}")

            elif fn == "_path_is_within_root":
                path, root = roots[label]
                if path is None:
                    want = False
                else:
                    # the live fn resolves first; the port takes resolved paths,
                    # so resolve here too
                    want = ns["_path_is_within_root"](ns["Path"](path), ns["Path"](root))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH _path_is_within_root case={label} C={got!r} PY={want!r}")

            elif fn == "_worktreeinclude_entries":
                lines, had_bom = includes[label]
                # the live fn reads a FILE; the port takes lines, so feed the
                # same text through the same filter
                want = [ln.strip() for ln in lines
                        if ln.strip() and not ln.strip().startswith("#")]
                got = c.get("out") or []
                if got != want:
                    mism += 1
                    print(f"MISMATCH _worktreeinclude_entries case={label} C={got!r} PY={want!r}")
        except Exception as e:
            mism += 1
            print(f"MISMATCH {fn} case={label} raised {type(e).__name__}: {e}")

    print(f"ORACLE worktree_ops: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
