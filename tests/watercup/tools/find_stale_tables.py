import os, re, json, glob

REPO = r'C:\Users\eman5\slermes'
HQ = r'C:\Users\eman5\hermes-agent'
GATES = os.path.join(REPO, 'tests', 'watercup', 'gates')

# parse C fn bodies
defs = {}
for path in glob.glob(os.path.join(REPO, 'src', '**', '*.c'), recursive=True):
    try:
        txt = open(path, encoding='utf-8', errors='replace').read()
    except Exception:
        continue
    for m in re.finditer(r'\n([A-Za-z_][\w \*]*?)\b([A-Za-z_]\w*)\s*\(([^;{)]*)\)\s*\{', txt):
        name = m.group(2); bs = m.end(); end = txt.find('\n}\n', bs)
        if end < 0 or end - bs > 4000: continue
        defs.setdefault(name, (path, txt[bs:end]))

STR = re.compile(r'"([^"\\]{2,40})"')
WORD = re.compile(r'[a-z][a-z0-9_\-]{2,}')

results = []
for gd in sorted(glob.glob(os.path.join(GATES, '*'))):
    gj = os.path.join(gd, 'gate.json')
    if not os.path.exists(gj): continue
    try: d = json.load(open(gj, encoding='utf-8'))
    except Exception: continue
    cfn = d.get('c_fn') or ''; pymod = d.get('py_module') or ''; pyfn = d.get('py_fn') or ''
    if cfn not in defs or not pymod: continue
    path, body = defs[cfn]
    # only string-literal-carrying bodies
    lits = set(STR.findall(body))
    # constant-ish tokens: lowercase word literals used for set membership (aliases/tables)
    toks = {l for l in lits if re.fullmatch(r'[a-z][a-z0-9_\-]{2,}', l)}
    if len(toks) < 3: continue
    pypath = os.path.join(HQ, *pymod[:-3].split('/') if pymod.endswith('.py') else pymod.split('/'))
    if not os.path.exists(pypath):
        pypath += '.py'
    if not os.path.exists(pypath): continue
    pytxt = open(pypath, encoding='utf-8', errors='replace').read()
    missing = sorted(t for t in toks if f'"{t}"' not in pytxt and f"'{t}'" not in pytxt and t not in pytxt)
    if missing:
        results.append((len(missing), os.path.basename(gd), cfn, pymod, missing))

results.sort(reverse=True)
print('gates with C constants absent from python:', len(results))
for n, g, cfn, pm, miss in results[:30]:
    print(n, '|', g, '|', cfn, '|', ','.join(miss[:8]))
open(os.path.join(REPO, '.testbin', 'stale_tables.txt'), 'w', encoding='utf-8').write(
    '\n'.join(f'{n}|{g}|{cfn}|{pm}|{",".join(m)}' for n, g, cfn, pm, m in results))
