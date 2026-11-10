#!/usr/bin/env python3
"""Oracle for port_update_cmd.c -- drives the LIVE hermes_cli/update_cmd.py defs.
The git/pip execution half is not imported; the pure defs are ast-extracted.

Note: most of update_cmd.py's defs are `stop_for_relaunch()` shims and are
deliberately not ported.
"""
import ast
import json
import os
import shlex
import subprocess
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

WIN_SEP = "\\"

HERMES_ROOT = r"C:\Users\eman5\hermes-agent"
if HERMES_ROOT not in sys.path:
    sys.path.insert(0, HERMES_ROOT)

UPD = r"C:\Users\eman5\hermes-agent\hermes_cli\update_cmd.py"
WANTED = ["_called_process_error_cmd_parts", "_called_process_error_is_git",
          "_called_process_error_is_python_dep_install",
          "_format_update_failure_stage", "_should_zip_fallback_on_update_error"]


def extract():
    ns = {"str": str, "int": int, "float": float, "bool": bool, "Any": object,
          "Optional": object, "NoReturn": object, "frozenset": frozenset,
          "list": list, "tuple": tuple, "dict": dict, "os": os, "shlex": shlex,
          "subprocess": subprocess, "BaseException": BaseException,
          "Exception": Exception}
    src = open(UPD, encoding="utf-8").read()
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            try:
                exec(compile(ast.Module(body=[node], type_ignores=[]), UPD, "exec"), ns)
            except Exception:
                pass
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), UPD, "exec"), ns)
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    class CPE:
        """A minimal CalledProcessError stand-in carrying .cmd."""
        def __init__(self, cmd):
            self.cmd = cmd

    parts_cases = {
        "argv": ["git", "pull"],
        "cmdline": "git pull --rebase",
        "padded": "  git   pull  ",
        "quoted": '"C:\\Program Files\\git.exe" pull',
        "empty": "",
        "null": None,
    }
    git_cases = {
        "git": ["git"], "git-exe": ["C:\\Program Files\\Git\\cmd\\git.exe"],
        "upper": ["GIT"], "python": ["python"], "empty": [],
    }
    dep_cases = {
        "pip": ["pip", "install", "foo"],
        "ensurepip": ["C:\\venv\\Scripts\\python.exe", "-m", "ensurepip"],
        "uv": ["uv", "pip", "install", "foo"],
        "uv-exe": ["C:\\venv\\Scripts\\uv.exe", "pip", "install"],
        "git": ["git", "pull"],
        "uninstall": ["pip", "uninstall", "foo"],
    }
    stage_cases = {"dep": dep_cases["pip"], "git": git_cases["git"],
                   "other": git_cases["python"]}
    zip_cases = {
        "all": (git_cases["git"], 1, 1, 1),
        "posix": (git_cases["git"], 1, 0, 1),
        "dirty": (git_cases["git"], 1, 1, 0),
        "dep": (dep_cases["pip"], 1, 1, 1),
        "other": (git_cases["python"], 1, 1, 1),
        "not-cpe": (git_cases["git"], 0, 1, 1),
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
            if fn == "_called_process_error_cmd_parts":
                v = parts_cases[label]
                if isinstance(v, list):
                    want = v
                elif v is None:
                    want = []
                else:
                    # posix=os.name != "nt" -> on this MSYS2 host os.name is
                    # "posix", so posix=True; the C harness passes 0 (Windows).
                    # Compare the WINDOWS form, which is what the port models.
                    want = shlex.split(v, posix=False) if v else []
                got = c.get("out") or []
                if got != want:
                    mism += 1
                    print(f"MISMATCH _called_process_error_cmd_parts case={label} C={got!r} PY={want!r}")

            elif fn == "_called_process_error_is_git":
                argv = git_cases[label]
                want = ns["_called_process_error_is_git"](CPE(argv if argv else None))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH _called_process_error_is_git case={label} C={got!r} PY={want!r}")

            elif fn == "_called_process_error_is_python_dep_install":
                argv = dep_cases[label]
                want = ns["_called_process_error_is_python_dep_install"](
                    CPE(argv if argv else None))
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH _called_process_error_is_python_dep_install case={label} C={got!r} PY={want!r}")

            elif fn == "_format_update_failure_stage":
                argv = stage_cases[label]
                want = ns["_format_update_failure_stage"](CPE(argv))
                got = c.get("out")
                if got != want:
                    mism += 1
                    print(f"MISMATCH _format_update_failure_stage case={label} C={got!r} PY={want!r}")

            elif fn == "_should_zip_fallback_on_update_error":
                argv, is_cpe, is_win, untouched = zip_cases[label]
                # run_checkout_untouched is a real VCS probe; the port models it
                # as a parameter, so the oracle passes the same value through.
                want = bool(is_cpe and is_win and
                            ns["_called_process_error_is_git"](CPE(argv)) and untouched)
                got = bool(c.get("out"))
                if got != want:
                    mism += 1
                    print(f"MISMATCH _should_zip_fallback_on_update_error case={label} C={got!r} PY={want!r}")
        except Exception as e:
            mism += 1
            print(f"MISMATCH {fn} case={label} raised {type(e).__name__}: {e}")

    print(f"ORACLE update_cmd: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
