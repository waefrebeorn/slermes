#!/usr/bin/env python3
"""Oracle for upstream Hermes lineage_adoption.py (w5_lineageadop_be48a0 gate).

The live module imports the persistence package tree; parent packages are
bootstrapped as namespace modules and the persistence leaf modules are
registered in sys.modules before the live file is executed.
"""
import importlib.util, json, sys
from types import ModuleType, SimpleNamespace as NS

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
PYROOT = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, PYROOT)

def _parent(name):
    m = sys.modules.get(name)
    if m is None:
        m = ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return m

for p in ("persistence", "persistence.conversation", "persistence.housekeeping"):
    _parent(p)

def _leaf(name, **attrs):
    m = ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m

class _TranscriptRead:
    def __init__(self, view=None):
        self.view = view

class _TranscriptView:
    ALL = "all"

_leaf("persistence.conversation.messages", TranscriptRead=_TranscriptRead,
      TranscriptView=_TranscriptView)
_leaf("persistence.conversation.sessions", END_REASON_ADOPTED="adopted")
_leaf("persistence.housekeeping.maintenance", ImportResult=object, SessionExport=object)

SRC = PYROOT + "/lineage_adoption.py"
spec = importlib.util.spec_from_file_location("lineageadop_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["lineageadop_live"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    if func == "result":
        d = r["result"]
        obj = NS(ok=d.get("ok"), imported=d.get("imported"), skipped=d.get("skipped"),
                 detached=d.get("detached"), imported_ids=d.get("imported_ids", []),
                 skipped_ids=d.get("skipped_ids", []), errors=d.get("errors", []))
        py = live._result_dict(obj)
    elif func == "entry":
        d = r["segment"]
        sdict = d.get("session", {})
        rows = d.get("messages", [])
        obj = NS(session=NS(as_dict=lambda sd=sdict: dict(sd)),
                 messages=[NS(as_wire_row=lambda m=m: m) for m in rows])
        py = live._import_entry(obj)
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w5_lineageadop_be48a0: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
