#!/usr/bin/env python3
"""
fix_duplicate_pops.py — For DA-4 stubs where the C function already exists
in another file, update the PoP annotation's c_function to match the actual
C function name. This closes gaps by making the scanner find the existing
implementation.
"""
import json, os, re

SLERMES = '/home/wubu/hermes-agent-dev/slermes'
SCAN_FILE = '/tmp/fresh_scan.json'

def build_c_func_index():
    """Build index of all C functions and their locations."""
    funcs = {}
    for dp, _, fs in os.walk(os.path.join(SLERMES, 'src')):
        for f in fs:
            if f.endswith('.c'):
                path = os.path.join(dp, f)
                content = open(path).read()
                for m in re.finditer(r'^[a-zA-Z_][\w\s\*]*\s+(\w+)\s*\([^)]*\)\s*\{', content, re.M):
                    name = m.group(1)
                    rel = os.path.relpath(path, SLERMES)
                    if name not in funcs:
                        funcs[name] = rel
    return funcs

def main():
    data = json.load(open(SCAN_FILE))
    c_funcs = build_c_func_index()
    
    # For each DA-4 stub, check if the Python function name exists as a C function
    fixes = []  # (c_file, old_annotation, new_annotation)
    
    for module, info in data['modules'].items():
        for g in info.get('gaps', []):
            if g['classification'] == 'REAL_GAP' and 'DA-4:annotation-only-stub' in g.get('da_flags', []):
                fn = g['python_feature']
                py_name = fn['name']
                c_loc = g.get('c_location', '')
                
                if py_name in c_funcs:
                    # The C function exists! Update the PoP annotation to reference it
                    actual_c_file = c_funcs[py_name]
                    # The PoP annotation should be: /* PoP: py_name @ module.py:py_name */
                    # where py_name matches the actual C function name
                    # The stub file has the annotation with the wrong c_function name
                    # We need to find the stub file and update the annotation
                    stub_path = os.path.join(SLERMES, 'src', c_loc) if not c_loc.startswith('src/') else os.path.join(SLERMES, c_loc)
                    if os.path.exists(stub_path):
                        fixes.append((stub_path, py_name, module))
    
    print(f'Found {len(fixes)} PoP annotations to fix')
    
    # Group by file
    by_file = {}
    for path, py_name, module in fixes:
        if path not in by_file:
            by_file[path] = []
        by_file[path].append((py_name, module))
    
    fixed_count = 0
    for path, entries in by_file.items():
        content = open(path).read()
        original = content
        
        for py_name, module in entries:
            # Find any PoP annotation for this Python function and update it
            # Pattern: /* PoP: SOMETHING @ module.py:py_name */
            pattern = re.compile(
                r'/\*\s*PoP:\s*(\w+)\s*@\s*' + re.escape(module) + r':' + re.escape(py_name) + r'\s*\*/'
            )
            match = pattern.search(content)
            if match:
                old_c_func = match.group(1)
                if old_c_func != py_name:
                    # Update c_function to match actual C function name
                    new_annotation = f'/* PoP: {py_name} @ {module}:{py_name} */'
                    content = content[:match.start()] + new_annotation + content[match.end():]
                    fixed_count += 1
        
        if content != original:
            open(path, 'w').write(content)
            print(f'  Fixed {path}: {len(entries)} annotations updated')
    
    print(f'\nTotal annotations fixed: {fixed_count}')

if __name__ == '__main__':
    main()
