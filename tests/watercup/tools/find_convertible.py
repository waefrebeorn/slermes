import os, re, json, glob, sys, importlib

REPO = r'C:\Users\eman5\slermes'
HQ = r'C:\Users\eman5\hermes-agent'
GATES = os.path.join(REPO, 'tests', 'watercup', 'gates')
sys.path.insert(0, os.path.join(REPO, 'tests', 'watercup'))

# reuse parsed defs
defs = {}
for path in glob.glob(os.path.join(REPO, 'src', '**', '*.c'), recursive=True):
    try:
        txt = open(path, encoding='utf-8', errors='replace').read()
    except Exception:
        continue
    for m in re.finditer(r'\n([A-Za-z_][\w \*]*?)\b([A-Za-z_]\w*)\s*\(([^;{)]*)\)\s*\{', txt):
        name = m.group(2)
        bs = m.end()
        end = txt.find('\n}', bs)
        if end < 0 or end - bs > 1500:
            continue
        defs.setdefault(name, (path, txt[bs:end]))

def logic_score(body):
    s = 0
    for tok in ('tolower(', 'strstr(', 'json_parse(', 'json_object(', 'for (', 'while (', 'strcmp('):
        s += body.count(tok)
    return s

def is_echo(body):
    if 'printf(' not in body and 'fprintf(' not in body:
        return False
    if 'return 0;' not in body:
        return False
    return True

sys.path.insert(0, HQ)
resolvable = {}
def py_ok(modpath, fn):
    key = modpath + '::' + fn
    if key in resolvable:
        return resolvable[key]
    ok = False
    try:
        parts = modpath[:-3].split('/') if modpath.endswith('.py') else modpath.split('/')
        modname = '.'.join(parts)
        m = importlib.import_module(modname)
        obj = getattr(m, fn, None)
        ok = callable(obj)
    except Exception:
        ok = False
    resolvable[key] = ok
    return ok

cands = []
for gd in sorted(glob.glob(os.path.join(GATES, '*'))):
    gj = os.path.join(gd, 'gate.json')
    if not os.path.exists(gj):
        continue
    try:
        d = json.load(open(gj, encoding='utf-8'))
    except Exception:
        continue
    cfn = d.get('c_fn') or ''
    pyfn = d.get('py_fn') or ''
    pymod = d.get('py_module') or ''
    if cfn not in defs:
        continue
    path, body = defs[cfn]
    if not is_echo(body):
        continue
    score = logic_score(body)
    if score < 2:
        continue  # pure echo, no real logic
    if not pyfn or not pymod:
        continue
    if '.' in pyfn:
        base = pyfn.split('.')[-1]
    else:
        base = pyfn
    if not py_ok(pymod, base):
        continue
    cands.append((score, os.path.basename(gd), cfn, pymod, pyfn, os.path.relpath(path, REPO)))

cands.sort(reverse=True)
print('convertible candidates:', len(cands))
for c in cands[:25]:
    print(c[0], '|', c[1], '|', c[2], '|', c[3], '::', c[4])
open(os.path.join(REPO, '.testbin', 'convertible.txt'), 'w', encoding='utf-8').write(
    '\n'.join('|'.join(map(str, c)) for c in cands))
