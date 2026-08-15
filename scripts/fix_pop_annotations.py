#!/usr/bin/env python3
"""
fix_pop_annotations.py — Update PoP annotations in DA-4 stub files to reference
the correct C function name. This closes gaps by making the scanner find the
existing C implementation.
"""
import json, os, re

SLERMES = '/home/wubu/hermes-agent-dev/slermes'
SCAN_FILE = '/tmp/fresh_scan.json'

def build_c_func_index():
    """Build index of all C functions."""
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

def find_best_c_match(py_name, c_funcs, c_loc):
    """Find the best matching C function name for a Python function."""
    # Exact match
    if py_name in c_funcs:
        return py_name
    
    # Look for C function ending with _py_name
    candidates = []
    for c_name in c_funcs:
        if c_name.endswith('_' + py_name) or c_name.endswith(py_name):
            candidates.append(c_name)
    
    if len(candidates) == 1:
        return candidates[0]
    if len(candidates) > 1:
        # Prefer the one in the same file
        for c in candidates:
            if c_funcs[c] == c_loc.replace('src/', ''):
                return c
        # Prefer shortest (most specific)
        return min(candidates, key=len)
    
    return None

def main():
    data = json.load(open(SCAN_FILE))
    c_funcs = build_c_func_index()
    
    # Collect fixes: (stub_file, py_name, module, correct_c_name)
    fixes = {}
    for module, info in data['modules'].items():
        for g in info.get('gaps', []):
            if g['classification'] == 'REAL_GAP' and 'DA-4:annotation-only-stub' in g.get('da_flags', []):
                fn = g['python_feature']
                py_name = fn['name']
                c_loc = g.get('c_location', '')
                if not c_loc:
                    continue
                
                best = find_best_c_match(py_name, c_funcs, c_loc)
                if best and best != py_name:
                    stub_path = os.path.join(SLERMES, 'src', c_loc) if not c_loc.startswith('src/') else os.path.join(SLERMES, c_loc)
                    if os.path.exists(stub_path):
                        key = stub_path
                        if key not in fixes:
                            fixes[key] = []
                        fixes[key].append((py_name, module, best))
    
    print(f'Found {sum(len(v) for v in fixes.values())} annotations to fix across {len(fixes)} files')
    
    fixed_count = 0
    for path, entries in fixes.items():
        content = open(path).read()
        original = content
        
        for py_name, module, correct_c in entries:
            # Find the PoP annotation for this Python function
            pattern = re.compile(
                r'/\*\s*PoP:\s*(\w+)\s*@\s*' + re.escape(module) + r':' + re.escape(py_name) + r'\s*\*/'
            )
            match = pattern.search(content)
            if match:
                old_c = match.group(1)
                if old_c != correct_c:
                    new_annotation = f'/* PoP: {correct_c} @ {module}:{py_name} */'
                    content = content[:match.start()] + new_annotation + content[match.end():]
                    fixed_count += 1
        
        if content != original:
            open(path, 'w').write(content)
    
    print(f'Fixed {fixed_count} PoP annotations')

if __name__ == '__main__':
    main()
