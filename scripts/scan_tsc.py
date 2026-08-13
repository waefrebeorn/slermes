#!/usr/bin/env python3
import json,subprocess
def scan(mod):
    r=subprocess.run(['python3','tests/slermes_parity_battleground.py','--module',mod,'--json'],
        capture_output=True,text=True,timeout=280,cwd='/home/wubu/hermes-agent-dev/slermes')
    if r.returncode!=0: return None
    d=json.loads(r.stdout)
    m=list(d['modules'].values())[0]
    return (m['ported'], m['real_gaps'], m['total'])

m=scan('tools/transcription_tools.py')
if m: print(f"transcription_tools.py: ported={m[0]} real_gaps={m[1]} total={m[2]}")
