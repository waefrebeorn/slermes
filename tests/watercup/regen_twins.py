#!/usr/bin/env python3
"""
regen_twins.py — regenerate all twin.py files using the current generator.

Fixes indentation issues from earlier broken generations, and passes the C
return type so the twin emits the SAME canonical string the C harness feeds
WCUP_VALSTR (otherwise every cup mismatches on encoding, not logic).
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "scripts"))

from gen_watercup_comprehensive import generate_twin

GATES = os.path.join(HERE, "gates")
POP_INDEX_PATH = os.path.join(HERE, "pop_index.json")

SIG_RE = re.compile(
    r'(?:static\s+)?(?:const\s+)?(?:unsigned\s+)?(?:long\s+)?(?:struct\s+\w+\s+)?'
    r'(\w+)\s+(\w+)\s*\(([^)]*)\)\s*\{'
)


def c_ret_type(pop_idx, py_module, py_fn):
    """Return (c_return_type, c_symbol) for the ported twin, or (None, None)."""
    info = pop_idx.get(f"{py_module}:{py_fn}")
    if not info:
        return None, None
    c_file, c_fn = info.get("c_file"), info.get("c_fn")
    if not c_file or not os.path.isfile(c_file):
        return info.get("ret_type"), c_fn
    try:
        txt = open(c_file, errors="ignore").read()
    except OSError:
        return info.get("ret_type"), c_fn
    for m in SIG_RE.finditer(txt):
        if m.group(2) == c_fn:
            return m.group(1), c_fn
    return info.get("ret_type"), c_fn


def main():
    with open(POP_INDEX_PATH) as f:
        pop_idx = {k.strip(): v for k, v in json.load(f).items()}

    fixed = 0
    skipped = 0
    for d in os.listdir(GATES):
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

        c_ret, c_fn = c_ret_type(pop_idx, py_module, py_fn)
        # The twin's Cup must open with the C symbol: Cup() folds the name
        # into the hash before any value, so a python-named cup can never
        # match a C-named harness cup even with identical traced values.
        cup_name = c_fn or py_fn
        new_content = generate_twin(py_module, py_fn, parent_class,
                                    c_ret=c_ret, cup_name=cup_name)

        old_content = open(tp).read()
        if old_content.strip() != new_content.strip():
            with open(tp, 'w') as f:
                f.write(new_content)
            fixed += 1
        else:
            skipped += 1

    print(f"Fixed {fixed} twin.py files, skipped {skipped} (already correct)")


if __name__ == "__main__":
    main()
