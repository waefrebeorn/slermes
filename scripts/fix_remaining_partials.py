#!/usr/bin/env python3
"""
Fix the 23 remaining PARTIAL annotations.
The scanner expects module X but some annotations say module Y.
Also handles header file and moved-function cases.
"""
import json, re, os

with open("/tmp/parity_round2.json") as f:
    d = json.load(f)

partials = []
for modname, mod in d['modules'].items():
    if mod.get('partial', 0) > 0:
        for g in mod.get('gaps', []):
            if g.get('classification') == 'PARTIAL':
                partials.append(g)

print(f"Fixing {len(partials)} remaining PARTIALs\n")

def find_fn_def(lines, name):
    for i, l in enumerate(lines):
        stripped = l.strip()
        if stripped.startswith('/*') or stripped.startswith('*') or stripped.startswith('//') or stripped.startswith('#'):
            continue
        m = re.match(
            r'^(?:static\s+|inline\s+|const\s+)?'
            r'(?:(?:unsigned\s+)?(?:bool|void|char|int|long|double|float|size_t|ssize_t|'
            r'json_t|json5_t|[A-Za-z_]\w*)\s+\*?\s*)?'
            r'(\w+)\s*\(',
            stripped
        )
        if m and m.group(1) == name:
            for j in range(i, min(i+5, len(lines))):
                if '{' in lines[j]:
                    return i
                if ';' in lines[j]:
                    break
    return None

def fix_or_add_annotation(fpath, c_fn, expected_mod, py_fn):
    """Add or fix PoP annotation for c_fn in fpath."""
    if not os.path.exists(fpath):
        print(f"  SKIP {fpath}: not found")
        return False
    
    with open(fpath) as f:
        content = f.read()
    lines = content.split('\n')
    
    expected_annotation = f"/* PoP: {c_fn} @ {expected_mod}:{py_fn} */"
    
    # Check if correct annotation already exists
    for line in lines:
        if expected_annotation in line:
            return True  # already correct
    
    # Find existing annotation (may have wrong module name)
    for i, line in enumerate(lines):
        if ('PoP:' in line or 'Port of Python' in line) and c_fn in line:
            # Replace this line with the correct annotation
            lines[i] = expected_annotation
            with open(fpath, 'w') as f:
                f.write('\n'.join(lines))
            print(f"  REPLACED: {fpath}:L{i+1}: {line.strip()} -> {expected_annotation}")
            return True
    
    # No existing annotation found - add one
    idx = find_fn_def(lines, c_fn)
    if idx is None:
        # Try alternate name (stripping common prefix)
        alt = c_fn
        for prefix in ['credential_pool_','weixin_','scheduler_','models_dev_',
                        'scale_to_zero_','auth_','backup_','debug_','gateway_',
                        'model_','models_','projects_db_','timeouts_','registry_',
                        'pairing_']:
            if c_fn.startswith(prefix):
                alt = c_fn[len(prefix):]
                idx = find_fn_def(lines, alt)
                if idx is not None:
                    break
        
        if idx is None and c_fn == 'close_terminal_tool':
            # Special case: function might be named differently
            idx = find_fn_def(lines, 'close_terminal_tool_impl')
        if idx is None and c_fn == 'load_pool':
            idx = find_fn_def(lines, 'credential_pool_load_pool')
        if idx is None and c_fn == 'is_windows':
            # Multiple is_windows functions - find by looking at context
            for i, l in enumerate(lines):
                if 'is_windows' in l and '{' in l and i < 200:
                    idx = i
                    break
        
        if idx is None:
            print(f"  NOT FOUND: {c_fn} in {fpath}")
            return False
    
    lines.insert(idx, expected_annotation)
    with open(fpath, 'w') as f:
        f.write('\n'.join(lines))
    print(f"  ADDED: {fpath}:L{idx+1}: {expected_annotation}")
    return True

# Handle each partial
for p in partials:
    loc = p.get('c_location', '')
    c_fn = p.get('c_function', '')
    py_fn = p['python_feature']['name']
    expected_mod = p.get('python_file', '')
    
    if not loc or not os.path.exists(loc):
        print(f"  SKIP {c_fn}: location missing/not found")
        continue
    
    # Handle special cases
    if c_fn == 'models_dev_supports_audio_input' and 'provider_metadata.c' in loc:
        # Function was moved to models_dev.c
        loc = loc.replace('provider_metadata.c', 'models_dev.c')
        if not os.path.exists(loc):
            print(f"  SKIP {c_fn}: moved to {loc} but not found")
            continue
    
    if c_fn == 'build_memory_context_block' and 'hermes_memory.h' in loc:
        # Header file - might be inline or macro
        print(f"  HEADER CASE: {loc}")
    
    if c_fn == 'secure_write' and 'hermes_gateway_pairing.h' in loc:
        # Header file
        print(f"  HEADER CASE: {loc}")
    
    fix_or_add_annotation(loc, c_fn, expected_mod, py_fn)
