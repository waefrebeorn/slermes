#!/usr/bin/env python3
"""Differential oracle for plugins/security-guidance/patterns.py (gate w19_patterns_a4e066).

The live module is pure data.  Its module-level assignments (extension tuples, the
three predicate lambdas, the reminder strings, SECURITY_PATTERNS) and the ``_rule``
builder are extracted from the AST and re-executed here; callable predicates are
compared as answer vectors over the fixture's probe paths.
"""
import ast, json, sys, __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = Path(__file__).resolve().parent.parent
SRC = REPO.parent / "hermes-agent" / "plugins" / "security-guidance" / "patterns.py"
GATE = "w19_patterns_a4e066"
_ANN = __future__.annotations.compiler_flag


def extract():
    tree = ast.parse(SRC.read_text(encoding="utf-8"))
    g = {"__name__": "patterns_live"}
    for node in tree.body:
        if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
            mod = ast.Module(body=[node], type_ignores=[])
            exec(compile(mod, str(SRC), "exec", flags=_ANN, dont_inherit=True), g)
        elif isinstance(node, ast.FunctionDef) and node.name == "_rule":
            mod = ast.Module(body=[node], type_ignores=[])
            exec(compile(mod, str(SRC), "exec", flags=_ANN, dont_inherit=True), g)
    return g


G = extract()
PATS = G["SECURITY_PATTERNS"]
BY_NAME = {p["ruleName"]: p for p in PATS}
JSD = G["_JS_ONLY"]
PYD = G["_PY_ONLY"]
NOTD = G["_NOT_DOCS"]


def run_case(r):
    f = r.get("func")
    if f == "js_only":
        return bool(JSD(r.get("path", "")))
    if f == "py_only":
        return bool(PYD(r.get("path", "")))
    if f == "not_docs":
        return bool(NOTD(r.get("path", "")))
    if f == "count":
        return len(PATS)
    if f == "names":
        return [p["ruleName"] for p in PATS]
    if f == "rule":
        return G["_rule"](r.get("name", ""), r.get("reminder", ""), **(r.get("triggers") or {}))
    if f == "pattern":
        entry = BY_NAME.get(r.get("name"))
        if entry is None:
            return None
        probes = r.get("probe_paths") or []
        out = {}
        for k, v in entry.items():
            if callable(v):
                out[k] = [bool(v(p)) for p in probes]
            else:
                out[k] = v
        return out
    return "<unknown>"


def brief(v):
    s = repr(v)
    return s if len(s) <= 240 else s[:240] + "...(%d chars)" % len(s)


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    f = r.get("func")
    if f not in {"js_only", "py_only", "not_docs", "count", "names", "rule", "pattern"}:
        continue
    cases += 1
    try:
        py = run_case(r)
    except BaseException as exc:
        mism += 1
        print("MISMATCH %s oracle raised %r args=%s" % (f, exc, brief(r)), file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s py=%s c=%s" % (f, brief(py), brief(got)), file=sys.stderr)

print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
sys.exit(1 if mism else 0)
