import sys, os, re, json
from pathlib import Path
sys.path.insert(0, 'tests')
import importlib.util
spec = importlib.util.spec_from_file_location("bg", "tests/slermes_parity_battleground.py")
bg = importlib.util.module_from_spec(spec); spec.loader.exec_module(bg)
a = bg.ParityAnalyzer(); ci = a.c_index; ext = bg.PythonExtractor()

py_files = []
for d in bg.PYTHON_SOURCE_DIRS.values():
    if isinstance(d, str): continue
    if d.is_file(): py_files.append(d)
    elif d.is_dir():
        for root, dirs, files in os.walk(d):
            for f in files:
                if f.endswith('.py'): py_files.append(Path(root) / f)

file2funcs = {}
for name, lst in ci.functions.items():
    for c in lst:
        file2funcs.setdefault(c.file, set()).add(name)

def tokens(s):
    s = re.sub(r'[A-Z]', lambda m: '_' + m.group(0).lower(), s)
    return set(t for t in re.split(r'[^a-z0-9]+', s.lower()) if len(t) >= 3)

pop_pyfiles = {}
for pop in ci.pop_annotations:
    for pf in pop.python_functions:
        pop_pyfiles.setdefault(pop.c_function, set()).add(pop.python_file)

def associated_cfiles(py_file):
    out = set()
    for cfile in file2funcs:
        if not cfile.startswith('src/') or not cfile.endswith('.c'): continue
        raw, _ = ci._get_cached_content(cfile)
        if py_file in raw or py_file.split('/')[-1].replace('.py', '') in cfile:
            if 'Port of Python' in raw or 'PoP:' in raw:
                out.add(cfile)
    return out

planned = []
checked = 0
for pf in py_files:
    rel = str(pf.relative_to(bg.HERMES_DIR))
    cfiles = associated_cfiles(rel)
    if not cfiles: continue
    cfuncs = set()
    for cf in cfiles:
        cfuncs |= file2funcs[cf]
    for feat in ext.extract_file(pf):
        if a.classify_feature(rel, feat).classification != 'REAL_GAP': continue
        checked += 1
        pname = feat.name.lstrip('_')
        ptoks = tokens(pname)
        if len(ptoks) < 1: continue
        best = None; bestscore = 0
        pseq = [t for t in re.sub(r'[A-Z]', lambda m: '_' + m.group(0).lower(), pname).lower().split('_') if len(t) >= 3]
        for cn in cfuncs:
            ctoks = tokens(cn)
            if not ctoks: continue
            inter = ptoks & ctoks
            # Subsequence (not necessarily contiguous) of python tokens within C tokens.
            cseq = [t for t in re.sub(r'[A-Z]', lambda m: '_' + m.group(0).lower(), cn).lower().split('_') if len(t) >= 3]
            it = iter(cseq)
            sub_ok = all(any(cur == tok for cur in it) for tok in pseq)
            if len(inter) >= 3 and sub_ok:
                score = len(inter)
                if score > bestscore:
                    bestscore = score; best = cn
        if best:
            linked = pop_pyfiles.get(best, set())
            if linked and rel not in linked:
                continue
            cfl = [cf for cf in cfiles if best in file2funcs[cf]]
            if cfl:
                planned.append((rel, feat.name, cfl[0], best, bestscore, sorted(ptoks & tokens(best))))

with open('/tmp/planned_token.json', 'w') as f:
    json.dump([p[:4] for p in planned], f)
print(f"Token candidates: {len(planned)} (checked {checked})")
from collections import Counter
for f_, n in Counter(p[2] for p in planned).most_common(15):
    print(f"  {n:3d}  {f_}")
print("\nSample:")
for p in sorted(planned, key=lambda x: -x[4])[:30]:
    print(f"  {p[4]} [{'|'.join(p[5])}]  {p[0]}:{p[1]} -> {p[2]}:{p[3]}")
