#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/run_status_filter (noc_w11_runstatus_8163ea.h)."""
import importlib.util
import json
import os
import sys
import types

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


if "agent.conversation_compression" not in sys.modules:
    cc = types.ModuleType("agent.conversation_compression")
    cc.CONTEXT_OVERFLOW_BLOCKED_WARNING_TEMPLATE = (
        "\u26a0 Context is over the compression threshold (~{tokens:,} tokens >= {threshold:,}) "
        "but compression is currently blocked ({reason}). The model may stop responding. Run /new to start a fresh "
        "session or /compress to retry immediately."
    )
    sys.modules["agent.conversation_compression"] = cc
live = _load("gateway/run_status_filter")


def _mk_chat_type(v):
    if isinstance(v, dict) and "value" in v:
        return types.SimpleNamespace(value=v["value"])
    return v


def compute(r):
    return live.hidden_outside_dm(_mk_chat_type(r["chat_type"]), r["message"])

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
        print(f"ORACLE w11_runstatus_8163ea: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w11_runstatus_8163ea {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w11_runstatus_8163ea: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
