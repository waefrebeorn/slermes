#!/usr/bin/env python3
"""regen_twins.py — regenerate twin.py files using the current generator.

Fixes twin drift after harness regeneration: the twin must encode the SAME
canonical values the C harness traces, under the SAME cup name. Passes the
port's real C signature (return type + param list) so the rich twin path
can mirror scalar out-params / char* buffer-outs and convert json/scalar
fixtures exactly like the harness does.

Usage:
  python3 tests/watercup/regen_twins.py              # all gates (idempotent)
  python3 tests/watercup/regen_twins.py <gate> ...    # only named gates
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# This file is <repo>/tests/watercup/regen_twins.py; the generator lives at
# <repo>/scripts/. (The historical "..", "scripts" join resolved to
# tests/scripts, which never existed — the tool could never run.)
sys.path.insert(0, os.path.join(HERE, "..", "..", "scripts"))

from gen_watercup_comprehensive import generate_twin

GATES = os.path.join(HERE, "gates")
POP_INDEX_PATH = os.path.join(HERE, "pop_index.json")

# Same signature regex fix_all_gates reads REAL signatures with: captures
# the pointer stars so "char **f(...)" parses as ret "char **", and the raw
# param text for a param_list split.
SIG_RE = re.compile(
    r'(?:static\s+)?(?:const\s+)?'
    r'((?:unsigned\s+long\s+long|unsigned\s+long|long\s+long|long|unsigned|[\w]+)'
    r'(?:\s*\*+)?)\s*(\w+)\s*\(([^)]*)\)\s*\{'
)


def _norm_ret(raw):
    """Normalize a C return type to the canonical twin-encoding name."""
    r = (raw or "").replace("*", "").strip()
    if r == "char":
        return "charptr"
    if r in ("json_t", "json_node_t"):
        return "json"
    return r


def c_signature(pop_idx, py_module, py_fn):
    """(c_ret, c_params, c_symbol) for the ported twin.

    Prefers the REAL C signature parsed out of the defining .c (the same
    source fix_all_gates regenerates harnesses from), falling back to the
    pop_index entry. A stale pop entry made every json/void-returning port
    encode as the wrong type; the real file is the truth.
    """
    info = pop_idx.get(f"{py_module}:{py_fn}") or {}
    c_file, c_fn = info.get("c_file"), info.get("c_fn")
    raw_ret, param_list = None, None
    if c_file and os.path.isfile(c_file):
        try:
            txt = open(c_file, errors="ignore").read()
        except OSError:
            txt = ""
        for m in SIG_RE.finditer(txt):
            if m.group(2) == c_fn:
                raw_ret = m.group(1).strip()
                params = m.group(3).strip()
                param_list = [p.strip() for p in params.split(",")
                             if p.strip()]
                break
    if raw_ret is None:
        raw_ret = info.get("ret_type")
        param_list = info.get("param_list")
    if param_list == ["void"]:
        param_list = []
    return _norm_ret(raw_ret), param_list, c_fn


def main():
    only = set(a for a in sys.argv[1:] if not a.startswith("-"))
    with open(POP_INDEX_PATH) as f:
        pop_idx = {k.strip(): v for k, v in json.load(f).items()}

    fixed = 0
    skipped = 0
    errors = 0
    for d in sorted(os.listdir(GATES)):
        if only and d not in only:
            continue
        gd = os.path.join(GATES, d)
        gj = os.path.join(gd, "gate.json")
        tp = os.path.join(gd, "twin.py")
        if not os.path.isfile(gj) or not os.path.isfile(tp):
            continue

        g = json.load(open(gj))
        py_module = g.get("py_module")
        py_fn = g.get("py_fn")
        parent_class = g.get("parent_class", "")
        if not py_module or not py_fn:
            continue

        c_ret, c_params, c_fn = c_signature(pop_idx, py_module, py_fn)
        # The twin's Cup must open with the C symbol: Cup() folds the name
        # into the hash before any value, so a python-named cup can never
        # match a C-named harness cup even with identical traced values.
        cup_name = c_fn or py_fn
        try:
            new_content = generate_twin(py_module, py_fn, parent_class,
                                        c_ret=c_ret, cup_name=cup_name,
                                        c_params=c_params)
        except Exception as e:
            errors += 1
            print(f"  ERROR {d}: {e}")
            continue

        old_content = open(tp).read()
        if old_content.strip() != new_content.strip():
            with open(tp, "w") as f:
                f.write(new_content)
            fixed += 1
        else:
            skipped += 1

    print(f"Fixed {fixed} twin.py files, skipped {skipped} (already correct),"
          f" errors {errors}")


if __name__ == "__main__":
    main()
