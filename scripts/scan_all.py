#!/usr/bin/env python3
"""Full scan: list modules by REAL_GAP, pick next heavy target."""
import subprocess, collections, sys, os
os.chdir('/home/wubu/hermes-agent-dev/slermes')

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
