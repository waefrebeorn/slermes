"""twin for curator_classify_removed_skills.

Traces LIVE agent/curator.py::_classify_removed_skills with the same rung
order as the C harness. Input: scenario JSON {removed, added, after, calls};
calls[] entries are skill_manage ARGUMENTS dicts (the twin wraps each in a
tool-call dict {"name": "skill_manage", "arguments": <args>}).

Rung order per removed skill (input order):
  B consolidated found / V "name->into" ("" into when pruned).
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

import importlib.util as ilu

spec = ilu.spec_from_file_location("wc_live_curator",
                                   os.path.join(DEV, "agent", "curator.py"))
_cur = ilu.module_from_spec(spec)
sys.modules["wc_live_curator"] = _cur
spec.loader.exec_module(_cur)

_classify = _cur._classify_removed_skills


def trace(scenario_json):
    c = Cup("classify_removed_skills")
    try:
        sc = json.loads(scenario_json)
    except ValueError:
        sc = {}
    if not isinstance(sc, dict):
        sc = {}
    removed = sc.get("removed") or []
    added = sc.get("added") or []
    after = set(sc.get("after") or [])
    tool_calls = [{"name": "skill_manage", "arguments": args}
                  for args in (sc.get("calls") or [])]
    result = _classify(removed, added, after, tool_calls)
    into_by_name = {e["name"]: e.get("into", "") for e in result["consolidated"]}
    for name in removed:
        name = name or ""
        into = into_by_name.get(name)
        c.branch(into is not None)               # B
        c.valstr("%s->%s" % (name, into or ""))  # V
    return c.finalize()


def run(inputs):
    return [trace(s) for s in inputs]
