#!/usr/bin/env python3
"""Oracle for literal constants of gateway/session_hosted_controls.py."""
import importlib, importlib.util, json, os, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, Q)
SRC = os.path.join(Q, 'gateway/session_hosted_controls' + ".py")
def _load_live():
    parts = 'gateway/session_hosted_controls'.split("/")
    for i in range(1, len(parts)):
        pname = ".".join(parts[:i])
        if pname in sys.modules: continue
        pdir = os.path.join(Q, *parts[:i])
        init = os.path.join(pdir, "__init__.py")
        sp = importlib.util.spec_from_file_location(pname, init, submodule_search_locations=[pdir])
        pm = importlib.util.module_from_spec(sp); sys.modules[pname] = pm
        try: sp.loader.exec_module(pm)
        except Exception: pass
    name = ".".join(parts)
    sp = importlib.util.spec_from_file_location(name, SRC)
    live = importlib.util.module_from_spec(sp); sys.modules[name] = live
    sp.loader.exec_module(live)
    return live
live = _load_live()
def compat(v):
    if isinstance(v, (set, frozenset)): return [compat(x) for x in sorted(v, key=repr)]
    if isinstance(v, (list, tuple)): return [compat(x) for x in v]
    if isinstance(v, dict): return {str(k): compat(x) for k, x in v.items()}
    if isinstance(v, int) and abs(v) > 9007199254740992: return str(v)
    return v
names = ["_GRANT_ERRORS"]
py = {n: compat(getattr(live, n)) for n in names}
cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    if r.get("func") != "constants": continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH cn_821f2df8af py={py!r} c={got!r}", file=sys.stderr)
print("ORACLE cn_821f2df8af: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
