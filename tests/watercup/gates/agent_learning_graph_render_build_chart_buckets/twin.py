"""twin for agent_learning_graph_render_build_chart_buckets.

Imports LIVE agent/learning_graph_render.py::_build_chart_buckets and traces
the same coarse branch ladder as the C harness (which links the REAL port):
  B1 nodes empty    B2 untimed
then emits one bucket summary V per run: "label|ts|skills|memories|rec|ids".
Input tokens are the same compact JSON objects the harness consumes.
"""
import json as _json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu

spec = ilu.spec_from_file_location(
    "wc_lgr", os.path.join(DEV, "agent", "learning_graph_render.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_lgr"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def _summary(buckets):
    parts = []
    for b in buckets:
        ids = ",".join(str(n.get("id", "?")) for n in b.nodes)
        parts.append("%s|%.6f|%d|%d|%.6f|%s" % (b.label, float(b.ts), b.skills,
                                                b.memories, b.rec, ids))
    return ";".join(parts)


def run(inputs):
    cups = []
    for raw in inputs:
        req = _json.loads(raw)
        nodes = req.get("nodes") or []
        rec = req.get("rec") or {}
        timed = bool(req.get("timed"))
        max_rows = int(req.get("max_rows", 1))
        c = Cup("build_chart_buckets")
        c.branch(len(nodes) == 0)                 # B1
        if nodes:
            c.branch(not timed)                   # B2
        if nodes:
            recd = {"rec": {str(k): v for k, v in rec.items()},
                    "timed": timed,
                    "minTs": req.get("minTs"),
                    "maxTs": req.get("maxTs")}
            buckets = mod._build_chart_buckets(nodes, recd, max_rows)
        else:
            buckets = []
        c.valstr(_summary(buckets))
        cups.append(c.finalize())
    return cups
