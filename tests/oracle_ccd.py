#!/usr/bin/env python3
"""Oracle for hermes_cli/cli_config_load.py::_cli_config_defaults.

The live function does `from hermes_cli.config_defaults import
DEFAULT_SANDBOX_IMAGE as img`. Ast-extract the function but seed the import via
a sys.modules fake whose module provides DEFAULT_SANDBOX_IMAGE, so the real
(heavy) config_defaults isn't imported. Then compare the json.dumps of the
result against the C serialization.
"""
import ast
import json
import sys
import types

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/cli_config_load.py"
IMG = "nousresearch/hermes-sandbox:desktop"


def main() -> int:
    fn_src = None
    for n in ast.parse(open(SRC, encoding="utf-8").read()).body:
        if isinstance(n, ast.FunctionDef) and n.name == "_cli_config_defaults":
            fn_src = ast.get_source_segment(open(SRC, encoding="utf-8").read(), n)
            break
    if fn_src is None:
        print("ORACLE ccd: _cli_config_defaults not found")
        return 1
    ns = {"sys": sys}
    exec(fn_src, ns)
    FN = ns["_cli_config_defaults"]

    fake = types.ModuleType("hermes_cli.config_defaults")
    fake.DEFAULT_SANDBOX_IMAGE = IMG
    sys.modules["hermes_cli.config_defaults"] = fake
    sys.modules["hermes_cli"] = types.ModuleType("hermes_cli")

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE ccd: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        if c.get("tag") != "ccd":
            continue
        total += 1
        want = FN()
        ws = json.dumps(want, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
        cs = json.dumps(json.loads(c["out"]), ensure_ascii=False, sort_keys=True,
                        separators=(",", ":"))
        if ws != cs:
            mism += 1
            print(f"MISMATCH {c.get('case')}\n  PY={ws}\n  C ={cs}")
    print(f"ORACLE ccd: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
