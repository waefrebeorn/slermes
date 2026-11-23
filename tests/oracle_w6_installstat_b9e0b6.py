#!/usr/bin/env python3
"""Oracle for pm/install_states.py (w6_installstat_b9e0b6)."""
import importlib.util, json, sys, os, pathlib, types, time
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = r"C:\Users\eman5\hermes-agent"
sys.path.insert(0, QUARRY)

def load(name, rel):
    parent = name.rsplit(".", 1)[0]
    if parent not in sys.modules:
        __import__(parent)
    spec = importlib.util.spec_from_file_location(name, os.path.join(QUARRY, *rel))
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m

live = load("pm.install_states", ["pm", "install_states.py"])
import hermes_cli.config as hcfg
import hermes_constants as hc

class FakePath:
    def __init__(self, s, exists=True, rel=False, pname="x", pdir=True):
        self.s = s; self._exists = exists; self._rel = rel
        self.parent = types.SimpleNamespace(name=pname, is_dir=lambda: pdir)
    def exists(self): return self._exists
    def is_relative_to(self, other): return self._rel

class Record:
    def __init__(self, isfile, text): self._isfile = isfile; self._text = text
    def is_file(self): return self._isfile
    def read_text(self, **kw): return self._text
class Inputs:
    def __init__(self, record): self.record = record
    def __truediv__(self, o): return self.record
class State:
    def __init__(self, isdir, record):
        self._isdir = isdir; self.record = record
    def is_dir(self): return self._isdir
    def __truediv__(self, o):
        return Inputs(self.record) if o == "inputs" else FakePath(self.s if hasattr(self, "s") else "/state")

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    try:
        if func == "pd":
            live.Path = FakePath
            root = FakePath("/chk", exists=bool(r["root_exists"]), rel=bool(r["root_rel"]),
                            pname=(".worktrees" if r["parent_worktrees"] else "x"),
                            pdir=bool(r["parent_isdir"]))
            py = bool(live._provably_deleted(root, FakePath("/data")))
        elif func == "orph":
            rec = Record(bool(r["record_isfile"]), r.get("record_root") or "")
            st = State(bool(r["state_isdir"]), rec)
            live.Path = lambda s, _re=bool(r["root_exists"]): FakePath(s, exists=_re)
            live._provably_deleted = lambda root, data_root: bool(r["provably"])
            live._last_used = lambda state: time.time() - float(r.get("age", 0))
            retention = r.get("retention")
            py = bool(live._is_orphan(st, FakePath("/data"), retention))
        elif func == "retention":
            hcfg.load_config_readonly = lambda: r["config"]
            hc.set_hermes_home_override = lambda p: "tok"
            hc.reset_hermes_home_override = lambda t: None
            py = live._retention_seconds(pathlib.Path("/data"))
            if py is not None: py = float(py)
        else:
            continue
    except Exception as e:
        mism += 1; cases += 1
        print(f"ORACLE w6_installstat_b9e0b6 ERROR case={r.get('case')} {e!r}", file=sys.stderr)
        continue
    got = r.get("out"); cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH w6_installstat_b9e0b6 case={r.get('case')} func={func} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_installstat_b9e0b6: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
