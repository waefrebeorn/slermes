"""Genuine reuse finder v2: precompute token sets for every Python function once,
then match REAL_GAP functions against already-credited (PORTED) functions by
token Jaccard >= 0.8 or token-subset. Avoids name-collision false matches."""
import sys, importlib.util, collections, re, ast
sys.path.insert(0,'tests')
spec=importlib.util.spec_from_file_location("bg","tests/slermes_parity_battleground.py")
bg=importlib.util.module_from_spec(spec); spec.loader.exec_module(bg)
a=bg.ParityAnalyzer(); ci=a.c_index
from pathlib import Path
PYTHON_SOURCE_DIRS=bg.PYTHON_SOURCE_DIRS

KW={'def','return','if','else','elif','for','while','in','not','and','or','is','None',
    'self','True','False','import','from','as','with','try','except','raise','lambda',
    'async','await','yield','class','pass','assert','global','nonlocal','del','print'}

def norm_tokens(src):
    src=re.sub(r'#.*','',src)
    src=re.sub(r'\"\"\".*?\"\"\"','',src,flags=re.DOTALL)
    toks=re.findall(r'[A-Za-z_][A-Za-z0-9_]*', src)
    return frozenset(t for t in toks if t not in KW)

# 1) precompute (module, func) -> tokens for ALL python functions
func_tokens={}  # (display, fname) -> frozenset
mod_files={}
for key,d in PYTHON_SOURCE_DIRS.items():
    if not (isinstance(d,Path) and d.is_dir()): continue
    for f in sorted(d.rglob("*.py")):
        if f.name=="__init__.py": continue
        display=f"{key}/"+str(f.relative_to(d))
        try: tree=ast.parse(f.read_text())
        except Exception: continue
        for node in ast.walk(tree):
            if isinstance(node,(ast.FunctionDef,ast.AsyncFunctionDef)):
                try: s=ast.get_source_segment(f.read_text(), node) or ''
                except Exception: s=''
                if s:
                    func_tokens[(display,node.name)]=norm_tokens(s)

# 2) credited: c_function -> list of (python_file, pyfunc)
cred=collections.defaultdict(list)
for pop in ci.pop_annotations:
    for pf in pop.python_functions:
        cred[pop.c_function].append((pop.python_file, pf))

# 3) gaps
gaps=[]
for key,d in PYTHON_SOURCE_DIRS.items():
    if not (isinstance(d,Path) and d.is_dir()): continue
    for f in sorted(d.rglob("*.py")):
        if f.name=="__init__.py": continue
        display=f"{key}/"+str(f.relative_to(d))
        for feat in a.extractor.extract_file(f):
            g=a.classify_feature(display,feat)
            if g.classification=='REAL_GAP':
                gaps.append((display,feat.name))

def jac(a,b):
    if not a or not b: return 0.0
    return len(a&b)/len(a|b)

matches=[]
for display,fname in gaps:
    g=func_tokens.get((display,fname))
    if not g or len(g)<4: continue
    for cf,entries in cred.items():
        for (pf,pyf) in entries:
            if not pf or pf==display: continue
            p=func_tokens.get((pf,pyf))
            if not p or len(p)<4: continue
            j=jac(g,p)
            if j>=0.8 or (g<=p or p<=g):
                matches.append((display,fname,cf,pf,round(j,2)))

seen=set(); uniq=[]
for m in matches:
    if m[:4] not in seen: seen.add(m[:4]); uniq.append(m)
print("GENUINE reuse matches:", len(uniq))
for display,fname,cf,pf,j in sorted(uniq,key=lambda x:-x[4]):
    print(f"  {j:.2f}  {display}:{fname}  ->  {cf} (credits {pf})")
