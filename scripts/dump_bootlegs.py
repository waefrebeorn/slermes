#!/usr/bin/env python3
"""Dump flagged bootleg function bodies for given C files (long-tail fixer aid)."""
import re, sys, json

def extract_fn(src, name):
    # find definition line
    m = re.search(r'^[A-Za-z_].*\b%s\s*\(' % re.escape(name), src, re.M)
    if not m:
        return None
    start = src.rfind('\n', 0, m.start()) + 1
    # include preceding comment lines
    lines_before = src[:start].split('\n')
    i = len(lines_before) - 2
    while i >= 0 and lines_before[i].strip().startswith(('/*', '*', '//')):
        if '*/' not in lines_before[i] or lines_before[i].strip().startswith('/*'):
            start = sum(len(l)+1 for l in lines_before[:i])
            i -= 1
            if lines_before[i+1].strip().startswith('/*') and lines_before[i+1].rstrip().endswith('*/') :
                break
        else:
            break
    # brace match
    i = src.find('{', m.end())
    depth = 0
    j = i
    while j < len(src):
        c = src[j]
        if c == '{': depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0: break
        j += 1
    return src[start:j+1]

q = json.load(open('tests/stub_fix_queue.json'))
byfile = {}
for e in q: byfile.setdefault(e['file'], []).append(e)

for path in sys.argv[1:]:
    try:
        src = open(path).read()
    except OSError as ex:
        print(f"### {path}: {ex}"); continue
    print(f"\n{'='*70}\n### {path}")
    for e in byfile.get(path, []):
        body = extract_fn(src, e['fn'])
        print(f"\n--- [{e['kind']}] {e['fn']}")
        print(body if body else "!! NOT FOUND IN FILE")
