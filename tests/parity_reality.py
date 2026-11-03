#!/usr/bin/env python3
"""Parity reality check — the honest counterpart to the battleground scanner.

`tests/slermes_parity_battleground.py` classifies a Python function as PORTED
when it finds a `/* PoP: ... */` comment naming it. The C function under that
comment may be `void f(void) { }`. So the gap count falls while nothing is
implemented, which is how ~250 commits of "gap closing" added no behaviour.

This tool refuses to score a placeholder as parity. A Python feature counts as
IMPLEMENTED only when a matching C definition exists AND its body does real
work:

  placeholder  body is empty, or is only `return 0/NULL/false;` / `(void)x;`
  implemented  anything else

Usage:
    python tests/parity_reality.py            # summary
    python tests/parity_reality.py --by-module
    python tests/parity_reality.py --placeholders
"""
import argparse
import ast
import pathlib
import re
import sys
from collections import defaultdict

HERMES = pathlib.Path("C:/Users/eman5/hermes-agent")
SLERMES = pathlib.Path("C:/Users/eman5/slermes")

PY_DIRS = {
    "agent": HERMES / "agent",
    "tools": HERMES / "tools",
    "gateway": HERMES / "gateway",
    "cron": HERMES / "cron",
    "hermes_cli": HERMES / "hermes_cli",
}

POP_RE = re.compile(r"/\*\s*PoP:\s*(\w+)\s*@\s*([\w/.]+):([\w.]+)\s*\*/")
# A C definition: name( ... ) { ... } spanning to a matching brace.
C_FUNC_RE = re.compile(
    r"\b(\w+)\s*\([^;{)]*\)\s*\{", re.MULTILINE
)

TRIVIAL_RET = re.compile(r"^\s*return\s+(-?\d+|NULL|0|1|false|true|-1)\s*;\s*$")
TRIVIAL_CAST = re.compile(r"^\s*\(\s*void\s*\)\s*\w+\s*;\s*$")


def python_features():
    """(module_path, func_name) for every function/method in the Python tree."""
    out = set()
    for mod, d in PY_DIRS.items():
        if not d.is_dir():
            continue
        for py in d.rglob("*.py"):
            try:
                tree = ast.parse(py.read_text(encoding="utf-8", errors="replace"))
            except SyntaxError:
                continue
            rel = py.relative_to(HERMES).as_posix()
            for node in ast.walk(tree):
                if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                    out.add((rel, node.name))
    return out


def extract_body(text, start):
    """Return the body text of the brace-delimited block starting at `start`."""
    depth = 0
    i = start
    while i < len(text):
        c = text[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return text[start + 1:i]
        i += 1
    return text[start + 1:]


def is_placeholder(body):
    stripped = body.strip()
    if not stripped:
        return True
    lines = [ln for ln in stripped.splitlines() if ln.strip()]
    if len(lines) > 3:
        return False
    for ln in lines:
        if not (TRIVIAL_RET.match(ln) or TRIVIAL_CAST.match(ln)
                or ln.strip() in ("{", "}")):
            return False
    return True


def c_definitions():
    """name -> (file, body, is_placeholder) for the first def of each name."""
    defs = {}
    for p in sorted(SLERMES.glob("src/**/*.c")) + sorted(SLERMES.glob("src/*.c")):
        text = p.read_text(encoding="utf-8", errors="replace")
        for m in C_FUNC_RE.finditer(text):
            name = m.group(1)
            if name in defs:
                continue
            body = extract_body(text, m.end() - 1)
            defs[name] = (p.relative_to(SLERMES).as_posix(), body,
                          is_placeholder(body))
    return defs


def c_names_by_pop():
    """(py_module, py_func) -> c_func from PoP comments."""
    out = {}
    for p in SLERMES.glob("src/**/*.c"):
        text = p.read_text(encoding="utf-8", errors="replace")
        for m in POP_RE.finditer(text):
            out.setdefault((m.group(2), m.group(3)), m.group(1))
    return out


def norm(s):
    return re.sub(r"[^a-z0-9]", "", s.lower())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--by-module", action="store_true")
    ap.add_argument("--placeholders", action="store_true")
    args = ap.parse_args()

    feats = python_features()
    defs = c_definitions()
    pops = c_names_by_pop()

    by_name = {norm(n): n for n in defs}

    implemented = defaultdict(int)
    placeholder = defaultdict(int)
    missing = defaultdict(int)
    ph_examples = []

    for mod, fn in feats:
        cname = pops.get((mod, fn)) or by_name.get(norm(fn)) or by_name.get(norm(mod.split("/")[-1][:-3] + "_" + fn))
        if cname is None or cname not in defs:
            missing[mod] += 1
            continue
        _file, body, is_ph = defs[cname]
        if is_ph:
            placeholder[mod] += 1
            if len(ph_examples) < 40:
                ph_examples.append(f"{mod}:{fn} -> {cname} ({_file})")
        else:
            implemented[mod] += 1

    tot_i = sum(implemented.values())
    tot_p = sum(placeholder.values())
    tot_m = sum(missing.values())
    total = tot_i + tot_p + tot_m

    print("Parity REALITY (placeholder bodies do not count)")
    print(f"  total Python features : {total}")
    print(f"  implemented           : {tot_i}  ({100.0*tot_i/total:.1f}%)")
    print(f"  placeholder only      : {tot_p}  ({100.0*tot_p/total:.1f}%)")
    print(f"  missing entirely      : {tot_m}  ({100.0*tot_m/total:.1f}%)")

    if args.by_module:
        print("\n  by top-level module:")
        agg = defaultdict(lambda: [0, 0, 0])
        for mod in set(list(implemented) + list(placeholder) + list(missing)):
            key = mod.split("/")[0]
            agg[key][0] += implemented[mod]
            agg[key][1] += placeholder[mod]
            agg[key][2] += missing[mod]
        for k in sorted(agg):
            i, p, m = agg[k]
            t = i + p + m
            print(f"    {k:12s} impl={i:5d} placeholder={p:5d} missing={m:5d}  ({100.0*i/t:.1f}% real)")

    if args.placeholders:
        print("\n  sample placeholders (PoP present, body does nothing):")
        for e in ph_examples:
            print("   ", e)

    return 0


if __name__ == "__main__":
    sys.exit(main())
