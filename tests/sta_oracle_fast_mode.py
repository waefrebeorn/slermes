#!/usr/bin/env python3
"""Oracle for the three pure service-tier helpers in agent/fast_mode.py.

The module only imports time/typing and has no heavy import chain, but we
ast-extract the three function defs plus the two lookup tables verbatim so the
oracle tracks source changes and needs no module import side effects.
"""
import ast
import json
import sys

SRC = "C:/Users/eman5/hermes-agent/agent/fast_mode.py"


def _extract(path, wants):
    ns = {}
    tree = ast.parse(open(path, encoding="utf-8").read())
    assigns = []
    funcs = []
    for n in tree.body:
        if isinstance(n, ast.Assign):
            ts = [t for t in n.targets]
        elif isinstance(n, ast.AnnAssign) and isinstance(n.target, ast.Name):
            ts = [n.target]
        else:
            ts = []
        if isinstance(n, ast.Assign) or (isinstance(n, ast.AnnAssign) and isinstance(n.target, ast.Name)):
            for t in ts:
                if isinstance(t, ast.Name) and (
                        t.id == "NORMAL_TIER_WORDS" or t.id == "SERVICE_TIER_WORDS"):
                    assigns.append(n)
        elif isinstance(n, ast.FunctionDef) and n.name in wants:
            funcs.append(n)
    # exec tables first, then the defs (the defs read the tables at call time).
    for n in assigns:
        exec(compile(ast.Module(body=[n], type_ignores=[]), "x", "exec"), ns)
    for n in funcs:
        exec(compile(ast.Module(body=[n], type_ignores=[]), "x", "exec"), ns)
    out = {}
    for n in assigns:
        vec = n.targets if isinstance(n, ast.Assign) else [n.target]
        for t in vec:
            if isinstance(t, ast.Name):
                out[t.id] = ns[t.id]
    for f in funcs:
        out[f.name] = ns[f.name]
    return out


NS = _extract(SRC, {"parse_service_tier", "parse_exact_service_tier", "service_tier_word"})
parse_service_tier = NS["parse_service_tier"]
parse_exact_service_tier = NS["parse_exact_service_tier"]
service_tier_word = NS["service_tier_word"]
NORMAL = NS["NORMAL_TIER_WORDS"]
SERVICE = NS["SERVICE_TIER_WORDS"]


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
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
        print("ORACLE fast_mode: 0 cases (link failure in disguise)")
        return 1
    mism = 0
    for c in cases:
        fam, raw = c["family"], c["in"]
        if fam == "pst":
            want = parse_service_tier(raw)
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH {c['case']}: C={c['out']!r} PY={want!r} in={raw!r}")
        elif fam == "pest":
            if "err" in c:
                try:
                    parse_exact_service_tier(raw)
                    mism += 1
                    print(f"MISMATCH {c['case']}: C=ERR PY=<no-raise> in={raw!r}")
                except ValueError:
                    pass
            else:
                want = parse_exact_service_tier(raw)
                if c["out"] != want:
                    mism += 1
                    print(f"MISMATCH {c['case']}: C={c['out']!r} PY={want!r} in={raw!r}")
        elif fam == "stw":
            want = service_tier_word(raw)
            if c["out"] != want:
                mism += 1
                print(f"MISMATCH {c['case']}: C={c['out']!r} PY={want!r} in={raw!r}")
    print(f"ORACLE fast_mode: {len(cases)} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
