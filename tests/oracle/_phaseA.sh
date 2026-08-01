#!/usr/bin/env bash
set -u
SL="/home/wubu/hermes-agent-dev/slermes"
cd "$SL"
python3 - <<'PY'
import json,subprocess,os
SL="/home/wubu/hermes-agent-dev/slermes"
todo=json.load(open("/tmp/todo.json"))
gen=os.path.join(SL,"tests/oracle/gen_oracle.py")
reg=json.load(open(os.path.join(SL,"tests/oracle/registry.json")))
simple_names=[]; counts={"SIMPLE":0,"NOPOP":0,"COMPLEX":0,"other":0,"NOCFILE":0}
for i,(py,cf) in enumerate(todo):
    if not cf or not os.path.isfile(os.path.join(SL,cf)):
        counts["NOCFILE"]+=1; continue
    r=subprocess.run(["python3",gen,py,cf,"--write"],capture_output=True,text=True)
    try: out=json.loads(r.stdout.strip().splitlines()[-1])
    except Exception: counts["other"]+=1; continue
    s=out.get("suitability")
    if s=="SIMPLE":
        counts["SIMPLE"]+=1; name=out["name"]
        reg["ports"][name]={"harness":out["harness"],"oracle":out["oracle"],"fixtures":"tests/oracle/fixtures/%s"%name}
        simple_names.append(name)
    elif s=="NOPOP": counts["NOPOP"]+=1
    elif s=="COMPLEX": counts["COMPLEX"]+=1
    else: counts["other"]+=1
json.dump(reg,open(os.path.join(SL,"tests/oracle/registry.json"),"w"),indent=2)
json.dump(simple_names,open("/tmp/simple_names.json","w"))
print("Phase A counts:",counts,"| registry after:",len(reg["ports"]),"| new SIMPLE:",len(simple_names))
PY
