#!/usr/bin/env python3
"""Oracle for hermes_cli/default_soul.py:is_legacy_template_soul.

Harness (t_port_default_soul.c) emits {t:"legacy", name, in:<raw text>, out:<0|1>}.
Replays LIVE is_legacy_template_soul on the emitted text. Ast-extracts the fn plus
its constants (_normalize_soul, _SCAFFOLD_HEAD, _SCAFFOLD_TAIL, _LEGACY_TEMPLATE_SOULS);
these Assigns reference each other so exec them in source order into one namespace.
Compare by converting the emitted bool to int.
"""
import ast
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/default_soul.py"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"Any": object}
    want_names = {"_normalize_soul", "_SCAFFOLD_HEAD", "_SCAFFOLD_TAIL",
                  "DEFAULT_SOUL_MD", "_LEGACY_TEMPLATE_SOULS",
                  "is_legacy_template_soul"}
    body = []
    for n in ast.parse(src).body:
        name = None
        if isinstance(n, ast.FunctionDef):
            name = n.name
        elif isinstance(n, ast.Assign) and isinstance(n.targets[0], ast.Name):
            name = n.targets[0].id
        elif isinstance(n, ast.AnnAssign) and isinstance(n.target, ast.Name):
            name = n.target.id
        if name in want_names:
            body.append(n)
    exec(compile(ast.Module(body=body, type_ignores=[]), SRC, "exec"), ns)
    f = ns["is_legacy_template_soul"]

    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = ast.literal_eval(line)
        except ValueError:
            continue
        total += 1
        want = 1 if f(c.get("in")) else 0
        got = int(c.get("out"))
        if want != got:
            mism += 1
            print(f"MISMATCH {c.get('name')} C={got} PY={want}")
    if total == 0:
        print("ORACLE default_soul: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE default_soul: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
