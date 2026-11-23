#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/session_lifecycle (noc_w12_sessionlif_36761a.h)."""
import importlib.util
import json
import os
import sys

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)


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


live = _load("gateway/session_lifecycle")
_ENV = "HERMES_AUTO_CONTINUE_FRESHNESS"


def compute(r):
    func = r["func"]
    if func == "is_delegate_child_row":
        return bool(live.is_delegate_child_row(r["row"]))
    if func == "auto_continue_freshness_window":
        raw = r.get("env_value")
        old = os.environ.get(_ENV)
        try:
            if raw is None:
                os.environ.pop(_ENV, None)
            else:
                os.environ[_ENV] = raw
            return live.auto_continue_freshness_window()
        finally:
            if old is None:
                os.environ.pop(_ENV, None)
            else:
                os.environ[_ENV] = old
    raise ValueError("unknown func " + str(func))


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
        print(f"ORACLE w12_sessionlif_36761a: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w12_sessionlif_36761a {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w12_sessionlif_36761a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
