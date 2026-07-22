#!/usr/bin/env python3
"""
Batch PoP annotation fixer for remaining PARTIALs.
Inserts /* PoP: c_function @ module:py_function */ before each C function
that the scanner reports as PARTIAL (exists but missing annotation).
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
                    'module': modname,
                    'py_function': py_fn,
                    'c_function': c_fn,
                    'c_location': c_loc,
                })

print(f"Total PARTIALs: {len(partials)}")

# Group by file
by_file = {}
for p in partials:
    loc = p['c_location']
    if loc and loc.startswith('src/'):
        fpath = loc
    elif loc and loc.startswith('include/'):
        fpath = loc
    else:
        print(f"  SKIP {p['c_function']}: no valid file path")
        continue
    by_file.setdefault(fpath, []).append(p)

# For each file, read it and add PoP annotations
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
    """Find the definition line of function `name` in the lines array."""
    # Match patterns like:
    #   static int name(...) {
    #   char *name(...) {
    #   int name(...)
    #   name(...)
    # But NOT:   /* ... name ... */  (comment)
    # But NOT:   name(...);  (forward declaration)
    # But NOT:   ->name(...) (struct access)
    for i, l in enumerate(lines):
        # Skip comments and strings
        stripped = l.strip()
        if stripped.startswith('/*') or stripped.startswith('*') or stripped.startswith('//'):
            continue
        if stripped.startswith('#') or stripped.startswith(';'):
            continue
        
        # Check if this line defines the function
        # Match: optional static/inline/const, return type, then name(
        m = re.match(
            r'^(?:static\s+|inline\s+|const\s+)?'
            r'(?:bool|void|char|int|long|unsigned|double|float|size_t|ssize_t|json_t|json5_t|hermes_|config_|' 
            r'[A-Za-z_]\w*\s+\*?)?'
            r'\s*'
            r'(\w+)\s*\(',
            stripped
        )
        if m and m.group(1) == name:
            # Must be a definition, not just a declaration
            # Look ahead for { on the same line or next few lines
            remainder = l
            for j in range(i, min(i+5, len(lines))):
                remainder = lines[j]
                if '{' in remainder:
                    return i
                if ';' in remainder:
                    break  # forward declaration, not a definition
    return None

total_added = 0
total_skipped = 0
total_errors = 0

for fpath, fns in sorted(by_file.items()):
    if not os.path.exists(fpath):
        print(f"  SKIP {fpath}: file not found")
        total_skipped += len(fns)
        continue
    
    with open(fpath) as f:
        content = f.read()
    lines = content.split('\n')
    
    # Sort fns by their position in file (for stable processing)
    positions = []
    for p in fns:
        idx = find_fn_def(lines, p['c_function'])
        if idx is not None:
            positions.append((idx, p))
        else:
            # Try alternate pattern: maybe the name is slightly different
            print(f"  NOT FOUND in {fpath}: {p['c_function']} (Py={p['py_function']})")
            total_skipped += 1
    
    # Process from bottom to top to preserve line numbers
    positions.sort(key=lambda x: x[0], reverse=True)
    
    modified = False
    for idx, p in positions:
        # Check if PoP annotation already exists
        # Look back a few lines for any existing annotation
        has_pop = False
        for k in range(max(0, idx-5), idx):
            if 'PoP:' in lines[k] or 'Port of Python' in lines[k]:
                has_pop = True
                break
        
        if has_pop:
            print(f"  ALREADY HAS ANNOTATION: {p['c_function']} in {fpath}")
            total_skipped += 1
            continue
        
        # Determine module name from module path
        mod = p['module']
        if mod.endswith('.py'):
            mod = mod[:-3]
        
        # Create PoP annotation
        pop_line = f"/* PoP: {p['c_function']} @ {mod}:{p['py_function']} */"
        
        # Check if the function has a preceding comment block
        # Insert pop_line right before the function definition
        lines.insert(idx, pop_line)
        modified = True
        total_added += 1
        print(f"  ADDED: {p['c_function']} @ {fpath}:L{idx+1}")
    
    if modified:
        try:
            with open(fpath, 'w') as f:
                f.write('\n'.join(lines))
        except Exception as e:
            print(f"  ERROR writing {fpath}: {e}")
            total_errors += 1

print(f"\n{'='*60}")
print(f"Total: {total_added} added, {total_skipped} skipped, {total_errors} errors")
