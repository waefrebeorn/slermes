#!/usr/bin/env python3
"""
WuBuMagic Startup Engine
Usage: python3 wubumagic.py <module_path>
Example: python3 wubumagic.py hermes_state.py
         python3 wubumagic.py gateway/platforms/api_server.py
         python3 wubumagic.py cli.py
"""
import sys, os, re, json, subprocess

SLERMES_DIR = os.path.dirname(os.path.abspath(__file__))
BASE_DIR = os.path.dirname(SLERMES_DIR)

def run(cmd, **kwargs):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, **kwargs)

def scan_gaps(module_filter=None):
    """Run the parity scanner and return REAL_GAP functions."""
    result = run(f"cd {SLERMES_DIR} && python3 tests/slermes_parity_battleground.py --json 2>/dev/null")
    if result.returncode != 0:
        print("ERROR: Scanner failed")
        print(result.stderr)
        return []
    
    data = json.loads(result.stdout)
    gaps = []
    for module, info in data['modules'].items():
        if module_filter and module_filter not in module:
            continue
        for g in info.get('gaps', []):
            if g['classification'] == 'REAL_GAP':
                gaps.append({
                    'module': module,
                    'name': g['python_feature']['name'],
                    'line': g['python_feature']['line_number'],
                    'reason': g.get('stub_reason', ''),
                })
    return gaps

def read_python_func(module, func_name):
    """Read a Python function's source from the Hermes codebase."""
    py_path = os.path.join(BASE_DIR, module)
    if not os.path.exists(py_path):
        return None
    
    lines = open(py_path).read().split('\n')
    for i, line in enumerate(lines):
        stripped = line.lstrip()
        if stripped.startswith(f'def {func_name}(') or stripped.startswith(f'async def {func_name}('):
            # Extract the full function
            func_lines = []
            base_indent = len(line) - len(stripped)
            for j in range(i, min(i + 200, len(lines))):
                l = lines[j]
                if j == i:
                    func_lines.append(l)
                    continue
                if l.strip() == '':
                    func_lines.append(l)
                    continue
                curr_indent = len(l) - len(l.lstrip())
                if curr_indent <= base_indent and l.strip():
                    break
                func_lines.append(l)
            return '\n'.join(func_lines)
    return None

def classify_leafness(func_body):
    """Classify how 'leaf' a function is (fewer internal calls = easier to port)."""
    if not func_body:
        return 'unknown'
    
    # Count calls to self.* methods (hard to port)
    self_calls = len(re.findall(r'self\.(\w+)\(', func_body))
    # Count calls to other modules
    module_calls = len(re.findall(r'(?:from|import)\s+(\w+)', func_body))
    # Count Python-specific patterns
    python_only = len(re.findall(r'yield|async |await |@|\*\*kwargs|\*args', func_body))
    
    score = self_calls * 3 + module_calls + python_only * 2
    if score == 0:
        return 'LEAF'
    elif score <= 3:
        return 'LOW'
    elif score <= 8:
        return 'MEDIUM'
    else:
        return 'HIGH'

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        print("\nAvailable modules with gaps:")
        gaps = scan_gaps()
        from collections import Counter
        mod_counts = Counter(g['module'] for g in gaps)
        for mod, cnt in mod_counts.most_common(20):
            print(f"  {cnt:4d}  {mod}")
        return
    
    target = sys.argv[1]
    print(f"\n{'='*60}")
    print(f"  WuBuMagic Engine — Target: {target}")
    print(f"{'='*60}\n")
    
    # Step 1: SCAN
    print("[1/5] Scanning for gaps...")
    gaps = scan_gaps(target)
    if not gaps:
        print(f"  No REAL_GAP functions found in '{target}'. Already fully ported!")
        return
    print(f"  Found {len(gaps)} REAL_GAP functions")
    
    # Step 2: PRIORITIZE
    print("\n[2/5] Prioritizing (leaf-first)...")
    for g in gaps:
        body = read_python_func(g['module'], g['name'])
        g['body'] = body
        g['leafness'] = classify_leafness(body)
    
    # Sort: LEAF first, then by line number
    leaf_order = {'LEAF': 0, 'LOW': 1, 'MEDIUM': 2, 'HIGH': 3, 'unknown': 4}
    gaps.sort(key=lambda g: (leaf_order.get(g['leafness'], 4), g['line']))
    
    from collections import Counter
    leaf_dist = Counter(g['leafness'] for g in gaps)
    print(f"  Leaf distribution: {dict(leaf_dist)}")
    
    # Step 3: REPORT
    print(f"\n[3/5] Gap analysis for '{target}':")
    print(f"  {'#':>3}  {'Leaf':>6}  {'Line':>5}  {'Name'}")
    print(f"  {'-'*3}  {'-'*6}  {'-'*5}  {'-'*30}")
    for i, g in enumerate(gaps):
        print(f"  {i+1:3d}  {g['leafness']:>6}  {g['line']:5d}  {g['name']}")
    
    # Step 4: READY
    print(f"\n[4/5] Ready to port.")
    print(f"\n  Next step: Pick a function and say:")
    print(f"    '~ WuBu ~ {target}::<func_name>'")
    print(f"  Or port all LEAF functions:")
    print(f"    '~ WuBu ~ {target}::all-leaf'")
    
    # Save state for the porting phase
    state_file = os.path.join(SLERMES_DIR, '.wubumagic_state.json')
    with open(state_file, 'w') as f:
        json.dump({'target': target, 'gaps': gaps}, f, indent=2)
    print(f"\n  State saved to {state_file}")

if __name__ == '__main__':
    main()
