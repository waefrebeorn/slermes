#!/usr/bin/env python3
"""Compare the C tool registry against the Python product's builtin tool names.

`toolsets.BUILTIN_TOOL_NAMES` is the contract a model calls against: a tool registered in C under a
different name is invisible to it, and a tool that exists only in Python is a porting gap. This
script measures both instead of guessing, and exits non-zero when a contract name is missing.

Usage:  python scripts/check_tool_parity.py [--json]
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
EXE = REPO / "slermes.exe"
PY_QUARRY = Path(os.environ.get("HERMES_QUARRY", r"C:/Users/eman5/hermes-agent"))
PY_BIN = PY_QUARRY / ".venv" / "Scripts" / "python.exe"


def c_tool_names() -> list[str]:
    # The binary needs its DLLs; the loader looks at PATH first, and a winlibs-only PATH has no
    # curses/panel, which silently yields zero output and looks like an empty registry.
    env = dict(os.environ)
    mingw = r"C:/msys64/mingw64/bin"
    env["PATH"] = mingw + ";" + env.get("PATH", "")
    out = subprocess.run([str(EXE), "tools"], cwd=REPO, capture_output=True, text=True,
                         errors="replace", env=env)
    names = []
    lines = out.stdout.replace("\r", "").splitlines()
    if "Registered tools" not in out.stdout:
        sys.exit(f"'slermes tools' produced no registry (rc={out.returncode}): "
                 f"{(out.stdout or out.stderr).strip()[:200]}")
    for line in lines:
        if not line.startswith("  ") or " \u2014 " not in line:
            continue
        name = line.strip().split(" \u2014 ")[0].split(" [")[0]
        # A schema description containing " — " is not a tool name.
        if len(name) > 60 or " " in name:
            continue
        names.append(name)
    return names


def python_tool_names() -> list[str]:
    code = "import toolsets;print('\\n'.join(sorted(toolsets.BUILTIN_TOOL_NAMES)))"
    out = subprocess.run([str(PY_BIN), "-c", code], cwd=PY_QUARRY, capture_output=True, text=True)
    if out.returncode != 0:
        sys.exit(f"cannot read BUILTIN_TOOL_NAMES from {PY_QUARRY}: {out.stderr.strip()[:200]}")
    return [n for n in out.stdout.split() if n]


def main() -> int:
    if not EXE.exists():
        sys.exit(f"no {EXE} -- build first")
    c = c_tool_names()
    py = python_tool_names()
    missing = [n for n in py if n not in c]
    extra = [n for n in c if n not in py]

    if "--json" in sys.argv:
        print(json.dumps({"c": len(c), "python": len(py), "missing": missing, "extra": extra}, indent=2))
        return 0 if not missing else 1

    print(f"C registry     : {len(c)} tools")
    print(f"Python builtin : {len(py)} tools")
    print(f"missing in C   : {len(missing)}")
    for n in missing:
        print(f"    {n}")
    print(f"not in Python  : {len(extra)}")
    print("    " + ", ".join(sorted(extra)) if extra else "    (none)")
    ok = not missing
    print("\ntool parity: " + ("PASS" if ok else f"FAIL ({len(missing)} builtin names unreachable)"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
