#!/usr/bin/env python3
"""Differential oracle for gate w14_sessionaut_eb9edc (gateway/session_authorities.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w14_sessionaut_eb9edc"
sys.path.insert(0, QUARRY)
NS = types.SimpleNamespace


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = sys.modules.get(".".join(parts[:i]))
        if parent is not None and not hasattr(parent, parts[i]):
            setattr(parent, parts[i], mod)
    return mod


for _pkg in ("gateway", "persistence", "persistence.core"):
    if _pkg not in sys.modules:
        _seed(_pkg)


class AuthorityRefusal(Exception):
    def __init__(self, reason):
        super().__init__(reason)
        self.reason = reason


_seed("hermes_constants",
      get_hermes_home=lambda: "/default",
      get_hermes_home_override=lambda: None,
      hermes_home_key=lambda h: str(h))
_seed("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)

SRC = QUARRY + "/gateway/session_authorities.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


def _auth(v):
    if isinstance(v, dict):
        return NS(profile_id=v.get("id"), retiring=bool(v.get("retiring", False)))
    return None


def compute(r):
    launch = r.get("launch_home", "")
    reg = live.SessionAuthorities(launch, multiplexed=bool(r.get("multiplexed_init", False)))
    for op in r.get("ops") or []:
        kind, home = op[0], op[1]
        if kind == "add":
            reg.add(home, _auth(op[2]), name=(op[3] if len(op) > 3 else None))
        elif kind == "replace":
            reg.replace(home, _auth(op[2]))
        elif kind == "remove":
            reg.remove(home)
    res = []
    for q in r.get("queries") or []:
        kind = q[0]
        if kind == "len":
            res.append(len(reg))
        elif kind == "contains":
            res.append(q[1] in reg)
        elif kind == "for_home":
            a = reg.for_home(q[1]); res.append(a.profile_id if a is not None else None)
        elif kind == "launch":
            a = reg.launch; res.append(a.profile_id if a is not None else None)
        elif kind == "multiplexed":
            res.append(reg.multiplexed)
        elif kind == "starting_homes":
            res.append(list(reg.starting_homes()))
        elif kind == "iter":
            res.append([a.profile_id for a in reg])
        elif kind == "serving":
            res.append([a.profile_id for a in reg.serving()])
        elif kind == "served_profiles":
            res.append([{"profile_id": a.profile_id, "home": a.profile_id} for a in reg.serving()])
        elif kind == "profile_ids":
            res.append(sorted(reg.profile_ids()))
        elif kind == "profile_name":
            res.append(reg.profile_name(NS(profile_id=q[1])))
        else:
            res.append(None)
    return res


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


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
        print(f"MISMATCH {G} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
