#!/usr/bin/env python3
"""Oracle for agent/turn_context.py:compression_made_progress.

Harness (t_port_agent_turn_context.c) emits {t:"prog", o,n,ot,nt, out} per case.
Replays LIVE compression_made_progress(orig_len,new_len,orig_tokens,new_tokens).
Pure boolean — no seeds beyond Any.
"""
import ast
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/turn_context.py"
FN = "compression_made_progress"


def main() -> int:
    src = open(SRC, encoding="utf-8").read()
    ns = {"Any": object}
    body = [n for n in ast.parse(src).body
            if isinstance(n, ast.FunctionDef) and n.name == FN]
    exec(compile(ast.Module(body=body, type_ignores=[]), SRC, "exec"), ns)
    f = ns[FN]

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
        want = 1 if f(int(c["o"]), int(c["n"]), int(c["ot"]), int(c["nt"])) else 0
        got = int(c["out"])
        if want != got:
            mism += 1
            print(f"MISMATCH o={c['o']} n={c['n']} ot={c['ot']} nt={c['nt']} C={got} PY={want}")
    if total == 0:
        print("ORACLE agent_turn_context: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE agent_turn_context: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
