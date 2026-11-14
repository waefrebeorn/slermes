#!/usr/bin/env python3
# --- repo/quarry roots (was a hard-coded /home/wubu/... path) ---
import os as _os
REPO = _os.path.dirname(_os.path.dirname(_os.path.abspath(__file__)))
def _find_quarry(_repo):
    """The Python quarry is the sibling dir that actually is Hermes."""
    _parent = _os.path.dirname(_repo)
    try:
        for _c in sorted(_os.listdir(_parent)):
            if _os.path.isfile(_os.path.join(_parent, _c, "pm", "environments.py")):
                return _os.path.join(_parent, _c)
    except OSError:
        pass
    return ""
PY_ROOT = _os.environ.get("HERMES_QUARRY", _find_quarry(REPO))
# --- end roots ---


"""Scan a single module for ported/real_gaps counts."""
import json,subprocess,sys,os
os.chdir(REPO)
mod = sys.argv[1] if len(sys.argv)>1 else 'agent/context_compressor.py'
r=subprocess.run(['python3','tests/slermes_parity_battleground.py','--module',mod,'--json'],
capture_output=True,text=True,timeout=290)
if r.returncode!=0:
    print("stderr:",r.stderr[-800:]); sys.exit(1)
d=json.loads(r.stdout)
m=list(d['modules'].values())[0]
print('%s: ported=%d real_gaps=%d total=%d' % (mod,m['ported'],m['real_gaps'],m['total']))
