#!/usr/bin/env python3
"""Differential oracle for gateway/host_attach.py (gate w6_hostattach_55be4c).

Three pure helpers are exercised against the LIVE module: the served-set
builder, HostGateway.serves and standalone_rescan_message. Process liveness /
control-socket polling is never touched; the two lazy profile-name lookups are
stubbed with seams mirrored in C.
"""
import importlib.util
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _stub(name, **attrs):
    parts = name.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pm = types.ModuleType(pn)
            pm.__path__ = [Q + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pm
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_stub("agent")
_stub("agent.deadline", poll_until=lambda *a, **k: None)
_stub("gateway.status",
      _profile_name_for_home=lambda p: p.name or "default",
      _same_hermes_home=lambda a, b: str(a) == str(b))
_stub("hermes_cli.profiles",
      normalize_profile_name=lambda name: (name or "default").strip().lower(),
      profile_is_standalone=lambda home: False)

LIVE_NAME = "w6_hostattach_55be4c_live"
spec = importlib.util.spec_from_file_location(LIVE_NAME, Q + "/gateway/host_attach.py")
live = importlib.util.module_from_spec(spec)
sys.modules[LIVE_NAME] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    f = r.get("func")
    if f == "served":
        py = list(live._served_from_identity(r.get("identity") or {}))
    elif f == "serves":
        g = live.HostGateway(
            1, Path("/home/u/.hermes/profiles/default"),
            tuple(r.get("profiles") or ()),
            served_known=bool(r.get("served_known", True)))
        py = bool(g.serves(r.get("profile", "")))
    elif f == "rescan_msg":
        py = live.standalone_rescan_message(r.get("profile", ""))
    else:
        continue

    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s args=%r\n  py=%r\n  c =%r" % (f, r, py, got), file=sys.stderr)

print("ORACLE w6_hostattach_55be4c: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
