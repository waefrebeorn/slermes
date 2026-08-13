#!/usr/bin/env python3
"""Scan a single module for ported/real_gaps counts."""
import json,subprocess,sys,os
os.chdir('/home/wubu/hermes-agent-dev/slermes')
mod = sys.argv[1] if len(sys.argv)>1 else 'agent/context_compressor.py'
r=subprocess.run(['python3','tests/slermes_parity_battleground.py','--module',mod,'--json'],
capture_output=True,text=True,timeout=290)
if r.returncode!=0:
    print("stderr:",r.stderr[-800:]); sys.exit(1)
d=json.loads(r.stdout)
m=list(d['modules'].values())[0]
print('%s: ported=%d real_gaps=%d total=%d' % (mod,m['ported'],m['real_gaps'],m['total']))
