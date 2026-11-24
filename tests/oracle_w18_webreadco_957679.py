#!/usr/bin/env python3
"""Oracle for gate w18_webreadco_957679 (hermes_cli/web_read_coalescing.py).

Ast-extracts the pure _freeze() def. Fixture markers rebuild Python container
types JSON cannot express (tuple/set/frozenset); the frozen (type, payload)
result is rendered canonically and compared to the C port's JSON rendering.
"""
import ast
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
SRC = QUARRY + "/hermes_cli/web_read_coalescing.py"

src = open(SRC, encoding="utf-8").read()
NS = {}
for node in ast.parse(src).body:
    if isinstance(node, ast.FunctionDef) and node.name == "_freeze":
        exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), NS)
FREEZE = NS["_freeze"]


def decode(v):
    if isinstance(v, dict):
        if len(v) == 1 and "__tuple__" in v:
            return tuple(decode(x) for x in v["__tuple__"])
        if len(v) == 1 and "__set__" in v:
            return set(decode(x) for x in v["__set__"])
        if len(v) == 1 and "__frozenset__" in v:
            return frozenset(decode(x) for x in v["__frozenset__"])
        return {k: decode(x) for k, x in v.items()}
    if isinstance(v, list):
        return [decode(x) for x in v]
    return v


def dump(x):
    return json.dumps(x, sort_keys=True, separators=(",", ":"))


def to_json(frozen):
    typ, payload = frozen
    n = typ.__name__
    if n == "dict":
        v = [[to_json(k), to_json(val)] for k, val in payload]
        v.sort(key=dump)
        return {"t": "dict", "v": v}
    if n in ("list", "tuple"):
        return {"t": n, "v": [to_json(e) for e in payload]}
    if n in ("set", "frozenset"):
        v = [to_json(e) for e in payload]
        v.sort(key=dump)
        return {"t": n, "v": v}
    if n in ("int", "float", "str", "bool"):
        return {"t": n, "v": payload}
    if n == "NoneType":
        return {"t": "NoneType", "v": None}
    raise AssertionError("unexpected frozen type " + n)


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except Exception:
            continue
        if r.get("func") != "freeze":
            continue
        try:
            py = to_json(FREEZE(decode(r.get("value"))))
        except Exception as exc:
            print(f"LIVE ERROR args={r}: {exc!r}", file=sys.stderr)
            continue
        cases += 1
        got = r.get("out")
        if py != got:
            mism += 1
            print(f"MISMATCH args={r} py={py!r} c={got!r}", file=sys.stderr)
    print(f"ORACLE w18_webreadco_957679: {cases} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
