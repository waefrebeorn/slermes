import json,subprocess,os
os.chdir('/home/wubu/hermes-agent-dev/slermes')
r=subprocess.run(['python3','tests/slermes_parity_battleground.py','--module','agent/context_compressor.py','--json'],capture_output=True,text=True,timeout=290)
d=json.loads(r.stdout); m=list(d['modules'].values())[0]
for g in sorted(m.get('gaps',[]), key=lambda x:str(x.get('name'))):
    print('  %s | c_fn=%s' % (g.get('name'), g.get('c_function')))
