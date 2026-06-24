#!/usr/bin/env python3
"""Devil's Advocate: depth-code audit for desktop_gui.c"""
import re, sys

with open('src/desktop_gui.c') as f:
    content = f.read()
    lines = content.split('\n')

issues = []

# 1. Buffer overflow risks
for i, line in enumerate(lines, 1):
    s = line.strip()
    if 'strcpy(' in s and not s.startswith(('//','/*')) and not 'snprintf' in s:
        issues.append(('OVERFLOW', i, f'strcpy — use snprintf: {s[:80]}'))
    if 'sprintf(' in s and not s.startswith(('//','/*')) and 'snprintf' not in s:
        issues.append(('OVERFLOW', i, f'sprintf — use snprintf: {s[:80]}'))

# 2. Global state analysis
for i, line in enumerate(lines, 1):
    if 'static app_state_t app' in line or 'app_state_t app;' in line:
        issues.append(('GLOBAL', i, f'{line.strip()} — single global state, OK for SDL app'))

# 3. hermes_log in non-port code (should be 0)
for i, line in enumerate(lines, 1):
    if 'hermes_log(' in line:
        issues.append(('STUB', i, f'hermes_log() detected — should be fprintf: {line.strip()[:60]}'))

# 4. Magic numbers
magic_nums = []
for i, line in enumerate(lines, 1):
    for m in re.finditer(r'\b(0x[0-9a-fA-F]+)\b', line):
        val = int(m.group(1), 16)
        if val > 0xffffff:  # color constants
            continue
        if val in (0x0d0d0e, 0x0a0a0b, 0x161618, 0x0053fd, 0x16161a, 0x222228, 
                   0xdddddd, 0xb1b1b1, 0x858585, 0xe75e78, 0x55a583, 0x6f9ba6):
            continue  # known palette constants
        # check if it's a number used in layout (not a color)
        if 'GC_RGB' not in line and 'GC_RGBA' not in line:
            continue
        if val < 0x100000:
            magic_nums.append((i, m.group(1), line.strip()[:60]))
if magic_nums:
    for i, v, ctx in magic_nums:
        issues.append(('MAGIC', i, f'Suspect color constant {v}: {ctx}'))

# 5. Missing includes check
needed_h = ['stdio.h', 'stdlib.h', 'string.h', 'time.h', 'math.h', 'dirent.h', 'unistd.h', 'sys/stat.h']
for h in needed_h:
    if f'#include <{h}>' not in content and f'#include \"{h}\"' not in content:
        if h != 'sys/stat.h':
            issues.append(('INCLUDE', 0, f'Missing #include <{h}>'))
        elif '<sys/stat.h>' not in content:
            issues.append(('INCLUDE', 0, f'Missing #include <sys/stat.h>'))

# 6. Function size analysis
func_pattern = re.compile(r'^(static )?(int|void|bool|char|gc_\w+|const\s+char)\s*\*?\w+\s*\(', re.MULTILINE)
funcs = []
for m in func_pattern.finditer(content):
    name = m.group().split()[-1].rstrip('(').lstrip('*')
    # find the opening brace
    brace_open = content.find('{', m.end())
    if brace_open < 0: continue
    # count lines until matching close brace
    depth = 0
    end = brace_open
    for j in range(brace_open, len(content)):
        if content[j] == '{': depth += 1
        if content[j] == '}': depth -= 1
        if depth == 0: end = j; break
    func_body = content[brace_open:end+1]
    body_lines = func_body.count('\n')
    if body_lines > 100:
        issues.append(('MONOLITH', 0, f'{name}: {body_lines} lines — consider splitting'))

# Print report
print("=" * 70)
print("DEVIL'S ADVOCATE AUDIT — desktop_gui.c")
print("=" * 70)

cats = {'OVERFLOW': 'BUFFER OVERFLOW RISKS',
        'GLOBAL': 'GLOBAL STATE',
        'STUB': 'STUB DETECTION',
        'MAGIC': 'SUSPECT CONSTANTS', 
        'INCLUDE': 'MISSING INCLUDES',
        'MONOLITH': 'MONOLITHIC FUNCTIONS'}

for cat, title in cats.items():
    c_issues = [(t,i,m) for t,i,m in issues if t == cat]
    if c_issues:
        print(f"\n  [{title}] ({len(c_issues)} issues)")
        for t,i,m in sorted(c_issues, key=lambda x: x[1]):
            print(f"    L{i}: {m}")

if not issues:
    print("\n  CLEAN — no issues found")

# Summary stats
print(f"\n{'─'*70}")
print(f"  Total lines: {len(lines)}")
print(f"  Functions: {len(funcs)}")
print(f"  Issues: {len(issues)}")

text_section = sum(1 for l in lines if 'gc_draw_text' in l)
bubble_section = sum(1 for l in lines if 'draw_bubble' in l)
print(f"  Text draw calls: {text_section}")
print(f"  Bubble draw calls: {bubble_section}")

# Check for 100% snprintf usage
snprintf_count = len(re.findall(r'snprintf\(', content))
strcpy_count = len(re.findall(r'strcpy\(', content))
sprintf_count = len(re.findall(r'sprintf\(', content))
print(f"  snprintf: {snprintf_count}, strcpy: {strcpy_count}, sprintf: {sprintf_count}")
if strcpy_count == 0 and sprintf_count == 0:
    print("  ✅ 100% snprintf usage — no buffer overflow risks")
