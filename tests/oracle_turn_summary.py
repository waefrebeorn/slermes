#!/usr/bin/env python3
"""Oracle for agent/turn_summary.py pure formatters + the tally pipeline.

Harness families (turn_summary), emitted per line:
  elapsed {family, arg:num, out}       -> format_elapsed(arg)
  plural  {family, n:int, noun, out}   -> _pluralize(n, noun)
  tokens  {family, n:int, out}         -> format_token_flow(n)
  summary {family, case, elapsed:num, max_segments:int,
           steps:[{tool,result,is_error}], out}
        -> rebuild a TurnTally by replaying the LIVE TurnSummaryCollector.record_tool
           over each step, then format_turn_summary(elapsed, tally, max_segments).

Ast-extracts the module's static constants ({,_} dictionary + summary prefix +
verb groups), the TurnTally + TurnSummaryCollector classes (so the exact verb
grouping / plural nouns / delta counting is reproduced), and the format helpers.
"""
import ast
import json
import sys
import types as _types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

SRC = "C:/Users/eman5/hermes-agent/agent/turn_summary.py"

# module bodies exec together in one unit so inter-dependent defs/classes live.
def load():
    src = open(SRC, encoding="utf-8").read()
    ns = {"Any": object}
    body = []
    for n in ast.parse(src).body:
        if isinstance(n, (ast.Assign, ast.AnnAssign)):
            targets = n.targets if isinstance(n, ast.Assign) else (n.target,)
            if any(isinstance(t, ast.Name) for t in targets):
                body.append(n)
        elif isinstance(n, ast.FunctionDef) and n.name in (
                "format_elapsed", "_pluralize", "format_token_flow",
                "format_turn_summary", "_extract_line_deltas"):
            body.append(n)
        elif isinstance(n, ast.ClassDef) and n.name in (
                "TurnTally", "TurnSummaryCollector"):
            body.append(n)
        elif isinstance(n, ast.ImportFrom):
            body.append(n)
        elif isinstance(n, ast.Import):
            body.append(n)
    code = compile(ast.Module(body=body, type_ignores=[]), SRC, "exec")
    exec(code, ns)
    return ns


def main() -> int:
    ns = load()
    fmt_elapsed = ns["format_elapsed"]
    pluralize = ns["_pluralize"]
    fmt_tokens = ns["format_token_flow"]
    Collector = ns["TurnSummaryCollector"]
    fmt_summary = ns["format_turn_summary"]

    total = 0
    mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            b = json.loads(line)
        except Exception:
            continue
        fam = b.get("family")
        total += 1
        if fam == "elapsed":
            want = fmt_elapsed(b.get("arg"))
            if b.get("out") != want:
                mism += 1
                print(f"MISMATCH elapsed arg={b.get('arg')} C={b.get('out')!r} PY={want!r}")
        elif fam == "plural":
            want = pluralize(b.get("n"), b.get("noun"))
            if b.get("out") != want:
                mism += 1
                print(f"MISMATCH plural n={b.get('n')} noun={b.get('noun')!r} "
                      f"C={b.get('out')!r} PY={want!r}")
        elif fam == "tokens":
            want = fmt_tokens(b.get("n"))
            if b.get("out") != want:
                mism += 1
                print(f"MISMATCH tokens n={b.get('n')} C={b.get('out')!r} PY={want!r}")
        elif fam == "summary":
            col = Collector()
            for step in b.get("steps") or []:
                col.record_tool(step.get("tool"),
                                result=step.get("result"),
                                is_error=bool(step.get("is_error")))
            want = fmt_summary(b.get("elapsed"), col.tally,
                               max_segments=b.get("max_segments"))
            if b.get("out") != want:
                mism += 1
                print(f"MISMATCH summary case={b.get('case')} "
                      f"C={b.get('out')!r} PY={want!r}")
    print(f"ORACLE turn_summary: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
