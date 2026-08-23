#!/usr/bin/env python3
"""Extract Python function source by name from the hermes-agent-dev quarry."""
import re, sys, os

PY_ROOT = '/home/wubu/hermes-agent-dev'

def find_files(rel=None):
    if rel:
        p = os.path.join(PY_ROOT, rel)
        return [p] if os.path.exists(p) else []
    out = []
    for d in ['agent', 'tools', 'gateway', 'plugins', 'hermes_cli', '.']:
        base = os.path.join(PY_ROOT, d)
        if not os.path.isdir(base):
            continue
        for root, dirs, files in os.walk(base):
            dirs[:] = [x for x in dirs if x not in ('.git', '__pycache__', 'node_modules', '.hermes-runtime')]
            for f in files:
                if f.endswith('.py'):
                    out.append(os.path.join(root, f))
    return out

def extract(fn_name, rel=None):
    files = find_files(rel)
    results = []
    pat = re.compile(r'^([ \t]*)def\s+' + re.escape(fn_name) + r'\s*\(', re.M)
    for path in files:
        try:
            src = open(path, encoding='utf-8', errors='replace').read()
        except OSError:
            continue
        m = pat.search(src)
        if not m:
            continue
        lines = src.split('\n')
        start = src[:m.start()].count('\n')
        indent = len(m.group(1))
        end = start + 1
        while end < len(lines):
            l = lines[end]
            if l.strip() and (len(l) - len(l.lstrip())) <= indent and not l.lstrip().startswith((')', ']')):
                break
            end += 1
        body = '\n'.join(lines[start:end])
        results.append((os.path.relpath(path, PY_ROOT), body))
    return results

if __name__ == '__main__':
    names = sys.argv[1:]
    for n in names:
        rs = extract(n)
        if not rs:
            print(f'=== {n}: NOT FOUND ===')
        for rel, body in rs:
            print(f'===== {n} @ {rel} =====')
            print(body)
            print()
