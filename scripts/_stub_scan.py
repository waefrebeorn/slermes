import os, re, subprocess
ROOT = os.getcwd()
SRC = os.path.join(ROOT, 'src')
# genuine "not implemented for a reason" markers
MARK = re.compile(r'not impl|not yet (ported|impl)|intentionally (not|un)ported|'
                  r'unimplemented|NYI|placeholder|/\* stub|// stub|'
                  r'reason.*(not|skip|omit|defer)|skipped (because|due|for)|'
                  r'left (unused|unimplemented|as a stub)|TODO: implement|'
                  r'XXX| FIXME: implement|for now.*(stub|empty|noop)', re.I)

hits = []
for dp, _, fs in os.walk(SRC):
    for fn in fs:
        if not fn.endswith('.c'): continue
        p = os.path.join(dp, fn)
        try: txt = open(p).read()
        except Exception: continue
        for i, line in enumerate(txt.splitlines(), 1):
            if MARK.search(line):
                hits.append((p, i, line.strip()[:120]))

print("=== Genuine 'not implemented / reason' markers:", len(hits))
for h in hits:
    print(f"  {h[0]}:{h[1]}: {h[2]}")

# Also: functions whose body is ONLY a placeholder return (single-statement stubs)
# Detect: a function def whose braces contain <=2 non-comment lines and a
# bare return of empty json/array/null/0 with no real work.
stub_re = re.compile(r'(\w[\w\s\*]*?)\s+(\w+)\s*\([^)]*\)\s*\{([^{}]*)\}', re.S)
stub_hits = []
for dp, _, fs in os.walk(SRC):
    for fn in fs:
        if not fn.endswith('.c'): continue
        p = os.path.join(dp, fn)
        try: txt = open(p).read()
        except Exception: continue
        for m in stub_re.finditer(txt):
            body = m.group(3)
            body_clean = re.sub(r'//.*|/\*.*?\*/', '', body).strip()
            lines = [l for l in body_clean.split(';') if l.strip()]
            if len(lines) <= 2 and re.search(r'return\s+(json_object|json_array|xstrdup\(""\)|NULL|0)\s*;', body_clean):
                # exclude legitimate null guards which usually have 'if'
                if 'if' not in body_clean:
                    stub_hits.append((p, m.group(2), body_clean.strip()[:100]))
print()
print("=== Single-statement placeholder stubs (no if-guard):", len(stub_hits))
for s in stub_hits[:60]:
    print(f"  {s[0]}: {s[1]}()  ->  {s[2]}")
