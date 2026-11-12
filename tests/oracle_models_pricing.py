#!/usr/bin/env python3
"""Oracle for hermes_cli/models_pricing.py pure pricing helpers.

The harness (t_port_models_pricing.c) emits one JSON line per case:
{"case":N,"fn":"price_float"|"sale_pct"|"format_price_per_mtok"|"strip_v1", ...}
"""
import ast
import json
import sys

SRC = "C:/Users/eman5/hermes-agent/hermes_cli/models_pricing.py"


def main() -> int:
    ns = {}
    want = {"_price_float", "_sale_pct", "_format_price_per_mtok", "_strip_v1"}
    tree = ast.parse(open(SRC, encoding="utf-8").read())
    nodes = [n for n in tree.body
             if isinstance(n, ast.FunctionDef) and n.name in want]
    # exec only the target functions: the module-level import of
    # hermes_cli.models_reasoning_caps is not resolvable from the oracle's cwd.
    exec(compile(ast.fix_missing_locations(ast.Module(body=nodes, type_ignores=[])),
                 SRC, "exec"), ns)
    price_float = ns["_price_float"]
    sale_pct = ns["_sale_pct"]
    fmt = ns["_format_price_per_mtok"]
    strip_v1 = ns["_strip_v1"]

    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fn = c.get("fn")
        if fn == "price_float":
            want = price_float(c["raw"], positive=bool(c["positive"]))
            got = c["out"]
            ok = (want is None and got is None) or (
                want is not None and got is not None and
                (want == got or (want != want and got != got)))
        elif fn == "sale_pct":
            want = sale_pct(c["current"], c["original"])
            got = c["out"]
            ok = (want is None and got == -1) or (want is not None and want == got)
        elif fn == "format_price_per_mtok":
            want = fmt(c["in"])
            got = c["out"]
            ok = want == got
        elif fn == "strip_v1":
            want = strip_v1(c["in"])
            got = c["out"]
            ok = want == got
        else:
            continue
        if not ok:
            mism += 1
            print(f"MISMATCH {c.get('case')} ({fn}): C={got!r} PY={want!r}")

    if total == 0:
        print("ORACLE models_pricing: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE models_pricing: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
