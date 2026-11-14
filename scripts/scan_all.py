#!/usr/bin/env python3
# --- repo/quarry roots (was a hard-coded /home/wubu/... path) ---
import os as _os
REPO = _os.path.dirname(_os.path.dirname(_os.path.abspath(__file__)))
def _find_quarry(_repo):
    """The Python quarry is the sibling dir that actually is Hermes."""
    _parent = _os.path.dirname(_repo)
    try:
        for _c in sorted(_os.listdir(_parent)):
            if _os.path.isfile(_os.path.join(_parent, _c, "pm", "environments.py")):
                return _os.path.join(_parent, _c)
    except OSError:
        pass
    return ""
PY_ROOT = _os.environ.get("HERMES_QUARRY", _find_quarry(REPO))
# --- end roots ---


"""Full scan: list modules by REAL_GAP, pick next heavy target."""
import subprocess, collections, sys, os
os.chdir(REPO)

# The scanner with no args prints human text; use --json which returns JSON.
r = subprocess.run(['python3','tests/slermes_parity_battleground.py','--json'],
                   capture_output=True, text=True, timeout=290)
if r.returncode != 0:
    print("scanner stderr:", r.stderr[-800:]); sys.exit(1)

import json
raw = r.stdout.strip()
d = json.loads(raw)
mods = d['modules']
rows = sorted([(mm['real_gaps'], k, mm['ported'], mm['total'])
               for k, mm in mods.items() if mm['real_gaps'] > 0],
              reverse=True)
tot = sum(mm['real_gaps'] for mm in mods.values())
print(f"=== TOTAL REAL_GAP (live): {tot} ===\n")
print("=== TOP 20 heavy modules still gapped ===")
for rg, k, po, t in rows[:20]:
    print(f"  {rg:3d} gaps | {po}/{t} ported | {k}")
