#!/usr/bin/env python3
"""Oracle for gateway/platform_activation.py pure activation core.

Harness families (gateway_platform_activation), emitted per line:
  yamlblocks {in:<json>, out:<json array or null>}  -> sorted(_yaml_block_names(in))
  matches    {name, cands:<json array>, ret}        -> _matches(name, cands)
  activation {name, blocks, pools, env, decl:<json arrays>, ret}
        -> evidence rule exactly as live activation_predicate's has_evidence:
           match blocks OR match pools, OR env-var prefix NAME_UPPER_ (dash->_),
           OR declared env name uppercased present in upper_env.
Ast-extracts _yaml_block_names + _matches from the live module and applies the
has_evidence rule verbatim (deleted-helper inline, so we can't ast-extract it).
"""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/gateway/platform_activation.py"

FUNS = ("_yaml_block_names", "_matches")


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {}
    for n in ast.parse(src).body:
        if isinstance(n, ast.FunctionDef) and n.name in FUNS:
            exec(compile(ast.Module(body=[n], type_ignores=[]), SRC, "exec"), ns)
    yaml_names = ns["_yaml_block_names"]
    matches = ns["_matches"]

    def evidence(name, blocks, pools, env, declared):
        if matches(name, blocks) or matches(name, pools):
            return True
        prefix = name.upper().replace("-", "_") + "_"
        upper_env = {e.upper() for e in env}
        if any(e.startswith(prefix) for e in upper_env):
            return True
        return any(d.upper() in upper_env for d in declared)

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
        print("ORACLE gateway_platform_activation: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    total = 0
    for c in cases:
        total += 1
        fam = c.get("family")
        if fam == "yamlblocks":
            got = c.get("out")
            yobj = c.get("in")
            want = sorted(yaml_names(yobj)) if isinstance(yobj, dict) else []
            got_sorted = sorted(got) if got else []
            if got_sorted != want:
                mism += 1
                print(f"MISMATCH yamlblocks case={c.get('case')} C={got} PY={want}")
        elif fam == "matches":
            want = matches(c.get("name"), c.get("cands"))
            if bool(c.get("ret")) != want:
                mism += 1
                print(f"MISMATCH matches case={c.get('case')} name={c.get('name')!r} "
                      f"cands={c.get('cands')} C={c.get('ret')} PY={want}")
        elif fam == "activation":
            want = evidence(c.get("name"),
                            c.get("blocks"), c.get("pools"),
                            c.get("env"), c.get("decl"))
            if bool(c.get("ret")) != want:
                mism += 1
                print(f"MISMATCH activation case={c.get('case')} name={c.get('name')!r} "
                      f"C={c.get('ret')} PY={want}")
    print(f"ORACLE gateway_platform_activation: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
