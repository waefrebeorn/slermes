#!/usr/bin/env python3
"""Register spec-loaded modules in sys.modules before exec_module.

113 oracles load a dev-tree module with spec_from_file_location. That does not
put the module in sys.modules, so any library that resolves the defining module
through sys.modules -- `@dataclass` does -- dies with

    AttributeError: 'NoneType' object has no attribute '__dict__'

and the port is reported ORACLE-BROKEN. That is an absent measurement wearing
the costume of a parity failure.

Inserts `sys.modules[<name>] = <modvar>` immediately before the exec, using the
name the spec was actually created with. Idempotent; syntax-checked.
"""
import ast
import re
import sys
from pathlib import Path

REPO = Path("C:/Users/eman5/slermes")
SPEC = re.compile(r"(\w+)\s*=\s*[\w.]*spec_from_file_location\(\s*(['\"])([\w.]+)\2")
EXEC = re.compile(r"^(\s*)(\w+)\.loader\.exec_module\((\w+)\)", re.M)


def fix(path):
    src = path.read_text(encoding="utf-8")
    out = src
    for m in EXEC.finditer(src):
        indent, specvar, modvar = m.group(1), m.group(2), m.group(3)
        # already registered?
        if re.search(rf"sys\.modules\[[^\]]+\]\s*=\s*{modvar}\b", out):
            continue
        sm = None
        for s in SPEC.finditer(out, 0, m.start()):
            if s.group(1) == specvar:
                sm = s
        if sm is None:
            continue
        name = sm.group(3)
        line = f"{indent}sys.modules[{name!r}] = {modvar}  # dataclasses need this"
        out = out[:m.start()] + line + "\n" + out[m.start():]
    if out == src:
        return 0
    ast.parse(out)
    path.write_bytes(out.encode("utf-8"))
    return 1


def main():
    targets = sorted((REPO / "tests").glob("sta_oracle_*.py"))
    if len(sys.argv) > 1:
        targets = [REPO / "tests" / a for a in sys.argv[1:]]
    n = sum(fix(f) for f in targets)
    again = sum(fix(f) for f in targets)
    print(f"oracles patched: {n}; second pass: {again} (must be 0)")
    return 0 if again == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
