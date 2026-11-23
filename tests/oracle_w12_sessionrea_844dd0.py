#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/session_reactions (noc_w12_sessionrea_844dd0.h)."""
import importlib.util
import json
import os
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)

from persistence.core.errors import AuthorityRefusal


def _load(modpath):
    parts = modpath.split("/")
    for i in range(1, len(parts)):
        pname = ".".join(parts[:i])
        if pname in sys.modules:
            continue
        pdir = os.path.join(QUARRY, *parts[:i])
        init = os.path.join(pdir, "__init__.py")
        sp = importlib.util.spec_from_file_location(pname, init, submodule_search_locations=[pdir])
        pm = importlib.util.module_from_spec(sp)
        sys.modules[pname] = pm
        try:
            sp.loader.exec_module(pm)
        except Exception:
            pass
    name = ".".join(parts)
    sp = importlib.util.spec_from_file_location(name + "_live", os.path.join(QUARRY, modpath + ".py"))
    live = importlib.util.module_from_spec(sp)
    sys.modules[name + "_live"] = live
    sp.loader.exec_module(live)
    return live


live = _load("gateway/session_reactions")


def compute(r):
    params = dict(r["params"])
    # Decimal-string row_id is the int64 transport seam; feed live a real int.
    if isinstance(params.get("row_id"), str):
        params["row_id"] = int(params["row_id"])
    try:
        rid, nr, em, au = live._parse(params)
    except AuthorityRefusal as exc:
        return {"refusal": exc.reason}
    return {"row_id": rid, "newest_role": nr, "emoji": em, "author": au}


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w12_sessionrea_844dd0: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w12_sessionrea_844dd0 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w12_sessionrea_844dd0: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
