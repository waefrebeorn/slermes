import json,subprocess,os
os.chdir('/home/wubu/hermes-agent-dev/slermes')
r=subprocess.run(['python3','tests/slermes_parity_battleground.py','--module','agent/context_compressor.py','--json'],capture_output=True,text=True,timeout=290)
d=json.loads(r.stdout)
m=list(d['modules'].values())[0]
print('=== %d (ported=%d, total=%d) ===' % (m['real_gaps'],m['ported'],m['total']))
names=[]
for g in m.get('gaps',[]):
    if g.get('classification')=='REAL_GAP':
        # infer name from c_function or the gap dict
        n=g.get('c_function') or g.get('name') or '(?)'
        names.append(n)
for n in sorted(set(names)): print('  ',n)
