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


import json,subprocess,os
os.chdir(REPO)
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
