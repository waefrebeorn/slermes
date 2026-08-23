"""twin for platforms_base_resolve_channel_skills.

Traces the LIVE gateway/platforms/base.py::resolve_channel_skills with the
SAME coarse rung ladder as the C harness (which drives the REAL C port), then
calls the LIVE function for the result value.

Input encoding: "<channel_id|->|<parent_id|->|<extra_json>"; "-" = absent.
"""
import os
import sys
import json as _json
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_platforms_base",
    os.path.join(DEV, "gateway", "platforms", "base.py"))
_pb = ilu.module_from_spec(_spec)
sys.modules["wc_live_platforms_base"] = _pb
if _spec.loader is not None:
    _spec.loader.exec_module(_pb)


def trace(extra, channel_id, parent_id):
    c = Cup("resolve_channel_skills")
    bindings = extra.get("channel_skill_bindings") or []
    ok = isinstance(bindings, list) and bool(bindings)
    c.branch(ok)                                              # B1
    if not ok:
        return c  # caller fills the V from LIVE result
    ids = set()
    if channel_id:
        ids.add(str(channel_id))
    if parent_id:
        ids.add(str(parent_id))
    c.branch(bool(ids))                                       # B2
    for entry in bindings:
        c.loop()
        eo = isinstance(entry, dict)
        c.branch(eo)
        if not eo:
            continue
        entry_id = str(entry.get("id", ""))
        match = entry_id in ids
        c.branch(match)
        if not match:
            continue
        skills = entry.get("skills") or entry.get("skill")
        if isinstance(skills, str):
            c.branch(True)                                    # B str
            break
        c.branch(False)
        lst = isinstance(skills, list) and bool(skills)
        c.branch(lst)                                         # B list
        if not lst:
            continue
        seen = []
        for name in skills:
            c.loop()
            is_str = isinstance(name, str)
            c.branch(is_str)
            if is_str:
                nm = name.strip()
                fresh = bool(nm) and nm not in seen
                c.branch(fresh)
                if fresh:
                    seen.append(nm)
        break
    return c  # caller fills the V


def run(inputs):
    cups = []
    for raw in inputs:
        ch_s, par_s, extra_s = raw.split("|", 2)
        channel = None if ch_s == "-" else ch_s
        parent = None if par_s == "-" else par_s
        try:
            extra = {} if extra_s == "null" else _json.loads(extra_s)
        except ValueError:
            extra = {}  # C json_parse fails -> NULL -> same empty behavior
        c = trace(extra, channel, parent)
        result = _pb.resolve_channel_skills(extra, channel, parent)
        c.valstr(",".join(result) if result is not None else "null")
        cups.append(c.finalize())
    return cups
