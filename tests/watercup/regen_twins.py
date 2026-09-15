#!/usr/bin/env python3
"""
regen_twins.py — regenerate all twin.py files using the current generator.
Fixes indentation issues from earlier broken generations.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "scripts"))

from gen_watercup_comprehensive import generate_twin

GATES = os.path.join(HERE, "gates")

fixed = 0
skipped = 0
for d in os.listdir(GATES):
    gd = os.path.join(GATES, d)
    gj = os.path.join(gd, "gate.json")
    tp = os.path.join(gd, "twin.py")
    if not os.path.isfile(gj):
        continue
    if not os.path.isfile(tp):
        continue

    g = json.load(open(gj))
    py_module = g.get("py_module")
    py_fn = g.get("py_fn")
    parent_class = g.get("parent_class", "")

    new_content = generate_twin(py_module, py_fn, parent_class)

    # Only write if content changed
    old_content = open(tp).read()
    if old_content.strip() != new_content.strip():
        with open(tp, 'w') as f:
            f.write(new_content)
        fixed += 1
    else:
        skipped += 1

print(f"Fixed {fixed} twin.py files, skipped {skipped} (already correct)")
