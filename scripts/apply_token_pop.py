import json, re, os

with open('/tmp/planned_token.json') as f:
    planned = json.load(f)

# Manual exclusions: known false positives
EXCLUDE = {
    ("hermes_cli/commands.py", "telegram_menu_commands"),
}

# Recompute using same script but filter out excludes
with open('/tmp/planned_token_filtered.json', 'w') as f:
    json.dump([p for p in planned if (p[0], p[1]) not in EXCLUDE], f)

def find_fn_insertion(content, cfunc):
    lines = content.split('\n')
    for i, l in enumerate(lines):
        s = l.strip()
        if s.startswith('/*') or s.startswith('*') or s.startswith('//'):
            continue
        if re.search(r'\b' + re.escape(cfunc) + r'\s*\(', l):
            if l.rstrip().endswith(';'):
                continue
            for j in range(i, min(i + 5, len(lines))):
                if '{' in lines[j]:
                    return i
    return None

def inside_comment(lines, idx):
    in_block = False
    for j in range(idx):
        line = lines[j]
        k = 0
        while k < len(line):
            if not in_block:
                if line[k:k+2] == '/*':
                    in_block = True; k += 2; continue
                if line[k:k+2] == '//':
                    break
            else:
                if line[k:k+2] == '*/':
                    in_block = False; k += 2; continue
            k += 1
    return in_block

added = 0
with open('/tmp/planned_token_filtered.json') as f:
    planned = json.load(f)

for (py_file, py_func, c_file, c_func) in planned:
    if not c_file.startswith('src/'):
        c_file = 'src/' + c_file
    if not os.path.exists(c_file):
        continue
    with open(c_file) as fh:
        content = fh.read()
    if f'@ {py_file}:{py_func}' in content:
        continue
    lines = content.split('\n')
    idx = find_fn_insertion(content, c_func)
    if idx is None:
        print(f"NO FN {c_func} in {c_file}"); continue
    if inside_comment(lines, idx):
        j = idx
        while j < len(lines) and inside_comment(lines, j + 1):
            j += 1
        idx = j + 1
        while idx < len(lines) and lines[idx].strip().startswith('*'):
            idx += 1
    annotation = f"/* PoP: {c_func} @ {py_file}:{py_func} */"
    lines.insert(idx, annotation)
    with open(c_file, 'w') as fh:
        fh.write('\n'.join(lines))
    added += 1
    print(f"ADDED {c_file}:L{idx+1} -> {annotation}")

print(f"\nAdded: {added}")
