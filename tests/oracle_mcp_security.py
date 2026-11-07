#!/usr/bin/env python3
"""Oracle for hermes_cli/mcp_security.py pure IOC-scan helpers.

Harness (t_port_mcp_security.c) emits, per family:
  basename: {t:"basename", cmd:<str|null>, out:<str>}   -> _command_basename(cmd)
  inline:   {t:"inline",   args:<str|null>, out:<str>}   -> _inline_script(args)
  entry:    {t:"entry",    e:<"cmd\x1fargs\x1fenv1\x1fenv2">, out:<str>}
               -> rebuild live dict from \x1f parts, then _entry_text(dict)
Needs shlex.split (posix disabled on Windows) and os.path (ntpath backslash).
"""
import ast
import os
import shlex
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/mcp_security.py"
FUNS = ["_command_basename", "_inline_script", "_entry_text"]


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"shlex": shlex, "os": os, "Any": object}
    body = [n for n in ast.parse(src).body
            if isinstance(n, ast.FunctionDef) and n.name in FUNS]
    exec(compile(ast.Module(body=body, type_ignores=[]), SRC, "exec"), ns)

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(ast.literal_eval(line))
        except ValueError:
            pass
    if not cases:
        print("ORACLE mcp_security: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        t = c.get("t")
        want = None
        if t == "basename":
            want = ns["_command_basename"](c.get("cmd"))
        elif t == "inline":
            want = ns["_inline_script"](c.get("args"))
        elif t == "entry":
            parts = (c.get("e") or "").split("\x1f")
            p0 = parts[0] if len(parts) > 0 else ""
            p1 = parts[1] if len(parts) > 1 else ""
            env = {("k%d" % i): v for i, v in enumerate(parts[2:])}
            entry = {"command": p0, "args": p1, "env": env}
            want = ns["_entry_text"](entry)
        got = c.get("out")
        if want != got:
            mism += 1
            print(f"MISMATCH {t} want={want!r} C={got!r}")
    print(f"ORACLE mcp_security: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
