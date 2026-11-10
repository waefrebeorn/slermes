#!/usr/bin/env python3
"""Oracle for port_clipboard.c -- drives the LIVE clipboard.py defs.

The harness echoes its inputs from C. The live functions are ast-extracted;
the module's subprocess/platform half is not imported.
"""
import ast
import json
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

SRC = r"C:\Users\eman5\hermes-agent\hermes_cli\clipboard.py"
WANTED = ["is_remote_shell_session", "_macos_has_bitmap"]


def extract():
    src = open(SRC, encoding="utf-8").read()
    ns = {}
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    return ns


def main():
    ns = extract()
    cases = 0
    mism = 0

    ssh_cases = {
        "none": {},
        "connection": {"SSH_CONNECTION": "10.0.0.1 22"},
        "tty": {"SSH_TTY": "/dev/pts/3"},
        "client": {"SSH_CLIENT": "10.0.0.1 50000 22"},
        "empty-values": {"SSH_CONNECTION": "", "SSH_TTY": "", "SSH_CLIENT": ""},
    }
    bitmap_cases = [None, "", "«class PNGf»", "«class TIFF»", "«class furl»",
                    "«class PNGf» «class ICNS»"]

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        fn = c.get("fn")
        cases += 1

        if fn == "is_remote_shell_session":
            env = ssh_cases.get(c.get("case"), {})
            want = bool(ns["is_remote_shell_session"](env))
            got = bool(c.get("out"))
            if got != want:
                mism += 1
                print(f"MISMATCH is_remote_shell_session case={c.get('case')} C={got!r} PY={want!r}")

        elif fn == "_macos_has_bitmap":
            # the live fn raises TypeError on None ("in" of None); the C port
            # returns 0, so compare against the exception case as False
            if c["in"] is None:
                want = False
            else:
                want = bool(ns["_macos_has_bitmap"](c["in"]))
            got = bool(c.get("out"))
            if got != want:
                mism += 1
                print(f"MISMATCH _macos_has_bitmap in={c['in']!r} C={got!r} PY={want!r}")

    print(f"ORACLE clipboard: {cases} cases, {mism} mismatches")
    return 0 if mism == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
