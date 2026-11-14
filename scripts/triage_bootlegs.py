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


"""Bulk triage: for every queue entry, print C fn signature/body-summary plus
the Python source (via PoP annotation) so gaps can be classified fast."""
import re, json, sys, subprocess

PY=PY_ROOT
q=json.load(open('tests/stub_fix_queue.json'))
OWNED={'gw_session','gateway_runner','stream_consumer','mcp_tool','tirith','final_gaps','util_helpers','platforms_base','cron','context_engine','bedrock'}
def mine(e):
    f=e['file']; b=f.split('/')[-1]
    if 'hermes_cli/ports_wrappers' in f: return False
    if any(o in b for o in OWNED): return False
    return True

def get_pop(src, pos):
    seg = src[max(0,pos-800):pos]
    pops = re.findall(r'/\*\s*PoP:\s*\w+\s*@\s*([\w/.]+):(\w+)\s*\*/', seg)
    return pops[-1] if pops else None

def py_src(mod, fn):
    path = PY+'/'+mod
    if mod.startswith('src/'): return None
    try: txt=open(path).read()
    except OSError: return None
    m=re.search(r'(?:async )?def %s\s*\(' % re.escape(fn), txt)
    if not m: return '!! py fn not found in '+mod
    start=m.start()
    nxt=re.search(r'\n(?:def |class |# ={5,})', txt[m.end():])
    end=m.end()+(nxt.start() if nxt else 3000)
    body=txt[start:min(end,start+2200)]
    return body

out=open('REPO + '/bootleg_triage.txt'','w')
byfile={}
for e in q:
    if mine(e): byfile.setdefault(e['file'],[]).append(e)

for f, entries in sorted(byfile.items()):
    try: src=open(f).read()
    except OSError: continue
    for e in entries:
        name=e['fn']
        m=re.search(r'^[A-Za-z_].*\b%s\s*\(' % re.escape(name), src, re.M)
        out.write(f"\n{'='*80}\n## {f}:{name} [{e['kind']}]\n")
        if not m:
            out.write('C FN NOT FOUND\n'); continue
        pop=get_pop(src,m.start())
        out.write(f'PoP: {pop}\n')
        i=src.find('{',m.end()); d=0; j=i
        while j<len(src):
            if src[j]=='{':d+=1
            elif src[j]=='}':
                d-=1
                if d==0:break
            j+=1
        body=src[m.start():j+1]
        out.write('--- C BODY ---\n'+body[:1500]+'\n')
        if pop:
            b=py_src(pop[0],pop[1])
            out.write(f'--- PY {pop[0]}:{pop[1]} ---\n'+(b or 'n/a')+'\n')
out.close()
print('written REPO + '/bootleg_triage.txt'')
