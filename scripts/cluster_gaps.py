"""Cluster REAL_GAP functions by identical/near-identical normalized body.
Goal: find helper logic duplicated across MANY modules = the swaths a shared
mechanism can capture. Only reports clusters with >=3 members (real reuse).
"""
import sys, importlib.util, collections, re, ast
sys.path.insert(0,'tests')
spec=importlib.util.spec_from_file_location("bg","tests/slermes_parity_battleground.py")
bg=importlib.util.module_from_spec(spec); spec.loader.exec_module(bg)
a=bg.ParityAnalyzer()
from pathlib import Path
PYTHON_SOURCE_DIRS=bg.PYTHON_SOURCE_DIRS
KW={'def','return','if','else','elif','for','while','in','not','and','or','is','None',
    'self','True','False','import','from','as','with','try','except','raise','lambda',
    'async','await','yield','class','pass','assert','global','nonlocal','del','print'}

def norm_tokens(src):
    src=re.sub(r'#.*','',src); src=re.sub(r'\"\"\".*?\"\"\"','',src,flags=re.DOTALL)
    return frozenset(t for t in re.findall(r'[A-Za-z_][A-Za-z0-9_]*', src) if t not in KW)

# precompute (module,func)->tokens
ft={}
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
                if s: ft[(display,node.name)]=norm_tokens(s)

gaps=[]
for key,d in PYTHON_SOURCE_DIRS.items():
    if not (isinstance(d,Path) and d.is_dir()): continue
    for f in sorted(d.rglob("*.py")):
        if f.name=="__init__.py": continue
        display=f"{key}/"+str(f.relative_to(d))
        for feat in a.extractor.extract_file(f):
            if a.classify_feature(display,feat).classification=='REAL_GAP':
                gaps.append((display,feat.name))

def jac(a,b):
    if not a or not b: return 0.0
    return len(a&b)/len(a|b)

# build signature: (tokens) -> list of (module,func)
sig=collections.defaultdict(list)
for disp,fn in gaps:
    t=ft.get((disp,fn))
    if t and len(t)>=4:
        sig[t].append((disp,fn))

# clusters: group by token-set equality first, then near (>=0.85) within a bucket
clusters=[]
seen=set()
items=list(sig.items())
for t1,members1 in items:
    if id(t1) in seen: continue
    cluster=[(t1,members1)]
    seen.add(id(t1))
    for t2,members2 in items:
        if id(t2) in seen: continue
        if jac(t1,t2)>=0.85:
            cluster.append((t2,members2)); seen.add(id(t2))
    allm=[m for _,ms in cluster for m in ms]
    if len(allm)>=3:
        # representative tokens
        clusters.append((len(allm), allm, t1))

clusters.sort(key=lambda x:-x[0])
print("Clusters of >=3 identical/near-identical gap functions (swaths):", len(clusters))
for n,members,rep in clusters[:30]:
    mods=set(m[0] for m in members)
    print(f"\n=== {n} members across {len(mods)} modules (rep tokens: {sorted(rep)[:12]}) ===")
    for disp,fn in members[:12]:
        print(f"   {disp}:{fn}")
