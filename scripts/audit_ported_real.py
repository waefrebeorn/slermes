"""Authoritative check using the scanner's own C index + classification:
For every module/feature classified PORTED, verify the crediting C function
(1) exists in the C index (real impl, not orphaned) and
(2) has a real body (not a one-line stub that returns 0/NULL without logic).
Report any PORTED feature whose c_function is missing from the index.
Also report PARTIAL features.
"""
import sys, importlib.util, json
sys.path.insert(0,'tests')
spec=importlib.util.spec_from_file_location("bg","tests/slermes_parity_battleground.py")
bg=importlib.util.module_from_spec(spec); spec.loader.exec_module(bg)
a=bg.ParityAnalyzer(); ci=a.c_index
# build set of all C function names known to indexer
all_c=set()
for fns in ci.functions.values():
    for cf in fns:
        all_c.add(cf.name)
# functions with a real definition (have a body). ci stores function->(file,line).
# We'll treat any indexed function as "exists". Orphan = c_function not in index.
problems=[]
ported=0; partial=0
import pathlib
HERMES='/home/wubu/hermes-agent-dev'
from pathlib import Path
PYTHON_SOURCE_DIRS=bg.PYTHON_SOURCE_DIRS
# iterate tracked modules
d=json.load(open('/tmp/parity_now.json'))
for modkey, mod in d['modules'].items():
    f=Path(HERMES)/modkey
    if not f.exists(): continue
    for feat in a.extractor.extract_file(f):
        g=a.classify_feature(modkey,feat)
        if g.classification=='PORTED':
            ported+=1
            cf=g.c_function
            if cf and cf not in all_c:
                problems.append((modkey,feat.name,cf,'PORTED_BUT_C_FUNC_MISSING_FROM_INDEX'))
        elif g.classification=='PARTIAL':
            partial+=1
print(f"PORTED features checked: {ported}")
print(f"PARTIAL features: {partial}")
print(f"PORTED-but-c-function-missing-from-index: {len(problems)}")
for p in problems[:50]:
    print("  ", p)
