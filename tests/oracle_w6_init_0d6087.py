#!/usr/bin/env python3
"""Oracle for upstream Hermes gateway/__init__.py lazy export table."""
import importlib.util, json, os, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q="C:/Users/eman5/hermes-agent"; sys.path.insert(0,Q)
SRC=os.path.join(Q,"gateway/__init__.py")
sp=importlib.util.spec_from_file_location("gwinit_live",SRC)
live=importlib.util.module_from_spec(sp); sys.modules[sp.name]=live; sp.loader.exec_module(live)
cases=mism=0
for line in sys.stdin:
    line=line.strip()
    if not line: continue
    try: r=json.loads(line)
    except Exception: continue
    f=r.get("func"); name=r.get("name")
    if f=="all": py=list(live._EXPORTS)
    elif f=="target": py=live._EXPORTS.get(name)
    elif f=="has": py=name in live._EXPORTS
    else: continue
    cases+=1; got=r.get("out")
    if py!=got:
        mism+=1; print(f"MISMATCH {f} {name} py={py!r} c={got!r}",file=sys.stderr)
print("ORACLE gwinit: %d cases, %d mismatches" % (cases,mism))
sys.exit(1 if mism else 0)
