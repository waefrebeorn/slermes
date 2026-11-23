#!/usr/bin/env python3
"""Differential oracle for gate w13_kanbanwork_7d88c0 (hermes_cli/kanban_workflow.py)."""
import importlib.util
import json
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w13_kanbanwork_7d88c0"
sys.path.insert(0, QUARRY)

SRC = QUARRY + "/hermes_cli/kanban_workflow.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def build_wf(wf):
    if not wf:
        return live.DEFAULT_WORKFLOW
    cols = tuple(live.Column(key=c["key"], label=c["label"],
                             icon=c.get("icon", "?"), drag_target=c.get("drag_target", True))
                 for c in wf["columns"])
    man = live._frozen_manual({src: tuple(ds) for src, ds in wf["manual"].items()})
    return live.Workflow(cols, man)


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    w = build_wf(r.get("workflow"))
    func = r.get("func")
    if func == "keys":
        return list(w.keys())
    if func == "column":
        c = w.column(r.get("key"))
        if c is None:
            return None
        return {"key": c.key, "label": c.label, "icon": c.icon, "drag_target": c.drag_target}
    if func == "can_move":
        try:
            return w.can_move(r.get("src"), r.get("dst"))
        except ValueError as exc:
            return {"error": str(exc)}
    if func == "to_dict":
        return w.to_dict()
    if func == "validate":
        try:
            w.validate()
            return {"ok": True}
        except ValueError as exc:
            return {"error": str(exc)}
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
