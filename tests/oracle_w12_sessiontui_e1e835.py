#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/session_tui_reads (noc_w12_sessiontui_e1e835.h)."""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)


def _stub(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


# tui_reads binds authority_persistence and ConversationRequest at module scope.
_stub("gateway.session_persistence", authority_persistence=lambda *a, **k: None)
_stub("persistence.conversation.messages", ConversationRequest=object)


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


live = _load("gateway/session_tui_reads")

_RENDER = {None: "none", "_status": "status", "_context": "context"}


def compute(r):
    if r["func"] == "tui_read":
        src = r.get("policy_source")
        policy = None if src is None else types.SimpleNamespace(source=src)
        fn = live.tui_read(policy, r.get("name", ""), r.get("arg", ""))
        key = getattr(fn, "__name__", None)
        if key not in _RENDER:
            key = None
        return _RENDER[key]
    if r["func"] == "saved_route":
        rowdata = r.get("row")
        if not isinstance(rowdata, dict):
            row = None
        else:
            row = types.SimpleNamespace(model_config=rowdata.get("model_config"), model=rowdata.get("model"))
        provider, base_url, model = live._saved_route(row)
        return {"provider": provider, "base_url": base_url, "model": model}
    raise ValueError("unknown func " + str(r["func"]))


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
        print(f"ORACLE w12_sessiontui_e1e835: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w12_sessiontui_e1e835 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w12_sessiontui_e1e835: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
