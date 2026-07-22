#!/usr/bin/env python3
"""
Fix the 67 remaining PARTIAL annotations in slermes.
Surgically adds /* PoP: c_function @ module.py:py_function */ 
before each C function that the scanner reports as missing its annotation.

Uses precise brace-aware function finding.
"""
import json, re, sys, os

# Load parity data
with open("/tmp/parity_live.json") as f:
    data = json.load(f)

# Collect all PARTIAL gaps
partials = []
for modname, mod in data['modules'].items():
    if mod.get('partial', 0) > 0:
        for g in mod.get('gaps', []):
            if g.get('classification') == 'PARTIAL':
                py_fn = g['python_feature']['name']
                c_fn = g.get('c_function', '')
                c_loc = g.get('c_location', '')
                partials.append({
                    'py_module': modname,
                    'py_function': py_fn,
                    'c_function': c_fn,
                    'c_location': c_loc,
                })

print(f"Total PARTIALs to fix: {len(partials)}")

def brace_scan(s):
    """Brace delta of a line, ignoring //, /**/, and string/char literals."""
    d = 0
    in_s = False
    in_c = False
    esc = False
    i = 0
    while i < len(s):
        ch = s[i]
        if in_s:
            if esc: esc = False
            elif ch == '\\': esc = True
            elif ch == '"': in_s = False
            i += 1; continue
        if in_c:
            if ch == "'": in_c = False
            i += 1; continue
        if ch == '/' and i+1 < len(s) and s[i+1] == '/':
            break
        if ch == '/' and i+1 < len(s) and s[i+1] == '*':
            j = s.find('*/', i+2)
            if j == -1: break
            i = j + 2; continue
        if ch == '"': in_s = True
        elif ch == "'": in_c = True
        elif ch == '{': d += 1
        elif ch == '}': d -= 1
        i += 1
    return d

def find_fn_def(lines, name):
    """Find the definition line (0-indexed) of function `name`.
    Tries exact match, then ${name}(, then name at start of line.
    Returns None if not found."""
    for i, l in enumerate(lines):
        stripped = l.strip()
        # Skip comments and preprocessor directives
        if stripped.startswith('/*') or stripped.startswith('*') or stripped.startswith('//') or stripped.startswith('#'):
            continue
        
        # Try patterns:
        # 1. ^...name(...)  — at start of line (after optional static/type)
        # 2. name(...){ — all on one line
        # 3. *name(...) — pointer return
        
        # Pattern: optional return type + whitespace + name(
        m = re.match(
            r'^(?:static\s+|inline\s+|const\s+)?'  # optional storage/qualifier
            r'(?:(?:unsigned\s+)?(?:bool|void|char|int|long|double|float|size_t|ssize_t|'
            r'json_t|json5_t|[A-Za-z_]\w*)\s+\*?\s*)?'  # optional return type
            r'(\w+)\s*\(',
            stripped
        )
        if m and m.group(1) == name:
            # Verify it has a body (not just a forward decl)
            for j in range(i, min(i+5, len(lines))):
                if '{' in lines[j]:
                    return i
                if ';' in lines[j]:
                    break  # forward declaration, skip
    return None


# Group partials by file
by_file = {}
for p in partials:
    loc = p['c_location']
    if not loc or not os.path.exists(loc):
        print(f"  SKIP {p['c_function']}: location '{loc}' not found")
        continue
    by_file.setdefault(loc, []).append(p)

total_added = 0
total_not_found = []
total_already = []

for fpath, fns in sorted(by_file.items()):
    with open(fpath) as f:
        content = f.read()
    lines = content.split('\n')
    
    modified = False
    for p in fns:
        c_fn = p['c_function']
        py_fn = p['py_function']
        
        # Check if PoP annotation already exists (any format including old style)
        has_any_pop = False
        for line in lines:
            if ('PoP:' in line or 'Port of Python' in line) and c_fn in line:
                has_any_pop = True
                break
        
        if has_any_pop:
            total_already.append(c_fn)
            continue
        
        # Find the function definition
        idx = find_fn_def(lines, c_fn)
        if idx is None:
            # Special cases where the name doesn't match the C source
            # Try stripping common prefixes/suffixes
            alt_names = []
            if c_fn.startswith('credential_pool_'):
                alt_names.append(c_fn[len('credential_pool_'):])
            if c_fn.startswith('weixin_'):
                alt_names.append(c_fn[len('weixin_'):])
            if c_fn.startswith('scheduler_'):
                alt_names.append(c_fn[len('scheduler_'):])
            if c_fn.startswith('models_dev_'):
                alt_names.append(c_fn[len('models_dev_'):])
            if c_fn.startswith('scale_to_zero_'):
                alt_names.append(c_fn[len('scale_to_zero_'):])
            if c_fn.startswith('auth_'):
                alt_names.append(c_fn[len('auth_'):])
            if c_fn.startswith('backup_'):
                alt_names.append(c_fn[len('backup_'):])
            if c_fn.startswith('debug_'):
                alt_names.append(c_fn[len('debug_'):])
            if c_fn.startswith('gateway_'):
                alt_names.append(c_fn[len('gateway_'):])
            if c_fn.startswith('model_'):
                alt_names.append(c_fn[len('model_'):])
            if c_fn.startswith('models_'):
                alt_names.append(c_fn[len('models_'):])
            if c_fn.startswith('projects_db_'):
                alt_names.append(c_fn[len('projects_db_'):])
            if c_fn.startswith('timeouts_'):
                alt_names.append(c_fn[len('timeouts_'):])
            if c_fn.startswith('registry_'):
                alt_names.append(c_fn[len('registry_'):])
            if c_fn.startswith('pairing_'):
                alt_names.append(c_fn[len('pairing_'):])
            
            for alt in alt_names:
                idx = find_fn_def(lines, alt)
                if idx is not None:
                    break
            
            if idx is None:
                total_not_found.append(p)
                continue
        
        # Build the correct PoP annotation
        py_mod = p['py_module']
        if py_mod.endswith('.py'):
            py_mod = py_mod[:-3]  # Remove .py — wait, the format should HAVE .py
        pop_line = f"/* PoP: {c_fn} @ {p['py_module']}:{py_fn} */"
        
        lines.insert(idx, pop_line)
        modified = True
        total_added += 1
        print(f"  Added: {c_fn} @ {os.path.basename(fpath)}:L{idx+1}")
    
    if modified:
        with open(fpath, 'w') as f:
            f.write('\n'.join(lines))

print(f"\nResults: {total_added} added, {len(total_already)} already present")
print(f"Not found ({len(total_not_found)}):")
for p in total_not_found:
    print(f"  C={p['c_function']:30s}  Py={p['py_function']:25s}  in={p['c_location']}")
