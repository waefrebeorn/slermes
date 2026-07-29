#!/usr/bin/env python3
"""
Safely add PoP annotations to PARTIAL functions.
Places the PoP comment on its own line, preserving existing comment structure.
"""
import json, re, os, sys

slermes = '/home/wubu/hermes-agent-dev/slermes'

# Load live parity scan
d = json.load(open(f'{slermes}/live_parity_scan.json'))
mods = d.get('modules', {})

# Collect PARTIAL entries that need PoP
targets = {}
for mod, info in mods.items():
    for g in info.get('gaps', []):
        if g.get('classification') != 'PARTIAL':
            continue
        cfn = g.get('c_function', '')
        loc = g.get('c_location', '')
        py_name = g['python_feature']['name']
        if not cfn or not loc:
            continue
        full_path = f'{slermes}/{loc}' if not loc.startswith('/') else loc
        if not os.path.exists(full_path):
            continue
        key = (cfn, loc)
        if key not in targets:
            targets[key] = []
        targets[key].append((mod, py_name))

print(f"Files to annotate: {len(set(k[1] for k in targets.keys()))}")
print(f"Total annotations needed: {sum(len(v) for v in targets.values())}")

# Process each file
edited_files = set()

for (cfn, loc), py_list in targets.items():
    full_path = f'{slermes}/{loc}' if not loc.startswith('/') else loc
    if not os.path.exists(full_path):
        continue
    if full_path in edited_files:
        continue
    
    with open(full_path, 'r', encoding='utf-8', errors='replace') as f:
        lines = f.readlines()
    
    original = ''.join(lines)
    
    # Build PoP comment
    if len(py_list) == 1:
        py_name = py_list[0][1]
        mod_name = py_list[0][0]  # Keep as module/file.py format
        pop = f'/* PoP: {cfn} @ {mod_name}:{py_name} */\n'
    else:
        names = ','.join(py for _, py in py_list)
        pop = f'/* PoP: {cfn} @ {names} */\n'
    
    # Find the function definition line
    # Pattern: function name followed by (params) {
    for i, line in enumerate(lines):
        # Check if this line contains the function definition
        if re.search(r'\b' + re.escape(cfn) + r'\s*\([^)]*\)\s*\{', line):
            # Check if already has PoP comment in previous 5 lines
            has_pop = False
            for j in range(max(0, i-5), i):
                if 'PoP:' in lines[j] and cfn in lines[j]:
                    has_pop = True
                    break
            if has_pop:
                continue
            
            # Check if previous line ends with { (function on single line)
            if i > 0 and '{' in lines[i-1]:
                # Insert before the previous line
                lines.insert(i-1, pop)
            else:
                # Insert before current line
                lines.insert(i, pop)
            
            edited_files.add(full_path)
            print(f"  Annotated: {cfn} in {loc}")
            break
    
    # Write back if changed
    new_content = ''.join(lines)
    if new_content != original:
        with open(full_path, 'w', encoding='utf-8') as f:
            f.write(new_content)

print(f"\nTotal files edited: {len(edited_files)}")