#!/usr/bin/env python3
"""Differential oracle for upstream Hermes gateway/run_inbound_hooks (noc_w11_runinbound_903938.h)."""
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


def _stub_heavy():
    if "gateway" not in sys.modules:
        g = types.ModuleType("gateway")
        g.__path__ = [os.path.join(QUARRY, "gateway")]
        sys.modules["gateway"] = g
    stubs = (
        ("gateway.platforms", {}, [os.path.join(QUARRY, "gateway", "platforms")]),
        ("gateway.platforms.event", {"MessageEvent": type("MessageEvent", (), {})}, []),
        ("gateway.session", {"SessionSource": type("SessionSource", (), {}),
                             "build_session_context": lambda *a, **k: None}, []),
        ("agent.i18n", {"t": lambda *a, **k: ""}, []),
    )
    for full, attrs, paths in stubs:
        if full not in sys.modules:
            m = types.ModuleType(full)
            m.__dict__.update(attrs)
            if paths:
                m.__path__ = paths
            sys.modules[full] = m


_stub_heavy()
live = _load("gateway/run_inbound_hooks")


class Ev:
    def __init__(self, text, args):
        self.text = text
        self._args = args

    def get_command_args(self):
        return self._args


def compute(r):
    ev = Ev(r.get("text") or "", r.get("args") or "")
    res = live.GatewayInboundHooksMixin._hm_expand_alias_quick_command(ev, r["qcmd"])
    return {"command": res, "text": ev.text}

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
        print(f"ORACLE w11_runinbound_903938: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w11_runinbound_903938 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w11_runinbound_903938: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
