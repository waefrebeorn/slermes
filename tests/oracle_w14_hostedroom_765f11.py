#!/usr/bin/env python3
'''Differential oracle for gate w14_hostedroom_765f11 (session_rules/hosted_rooms.py).'''
import importlib.util
import json
import re
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w14_hostedroom_765f11"
sys.path.insert(0, QUARRY)


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


for _pkg in ("persistence", "persistence.gateway"):
    if _pkg not in sys.modules:
        _seed(_pkg)


class HostedRoomError(Exception):
    pass


class ReplicaError(Exception):
    pass


class ReplicaGapError(Exception):
    pass


class AuthorityHistoryError(Exception):
    pass


_seed("persistence.gateway.hosted_rooms", HostedRoomError=HostedRoomError,
      ReplicaError=ReplicaError, ReplicaGapError=ReplicaGapError,
      AuthorityHistoryError=AuthorityHistoryError)

SRC = QUARRY + "/session_rules/hosted_rooms.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)

IDENTIFIER_RE = live.IDENTIFIER_RE
EVENT_KIND_RE = re.compile(r"^[a-z][a-z0-9_.-]*$")


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    f = r.get("func")
    try:
        if f == "identifier":
            pm = r.get("pattern_mode", "identifier")
            pattern = IDENTIFIER_RE if pm == "identifier" else (EVENT_KIND_RE if pm == "event_kind" else None)
            return live.identifier(r.get("value"), label=r.get("label", "value"), error=HostedRoomError,
                                   max_chars=int(r.get("max_chars", 128)), pattern=pattern,
                                   invalid=r.get("invalid"))
        if f == "bounded_int":
            return live.bounded_int(r.get("value"), error=HostedRoomError, message=r.get("message", "invalid integer"),
                                    low=int(r.get("low", 0)), high=(None if r.get("high") is None else int(r["high"])))
        if f == "text":
            return live.text(r.get("value"), error=HostedRoomError, label=r.get("label", "value"),
                             max_bytes=int(r.get("max_bytes", 0)), strip=bool(r.get("strip", True)))
        if f == "validate_context":
            return live.validate_context(r.get("purpose"), r.get("notes"), r.get("routing", "everyone"),
                                         r.get("lead_member_id", ""))
        if f == "context_from_payload":
            return live.context_from_payload(r.get("payload"))
        if f == "retry_fields":
            return live.retry_fields(r.get("context") or {}, r.get("original") or {})
        if f == "retained_fields":
            return live.retained_fields(r.get("table"))
    except (HostedRoomError, ReplicaError, ReplicaGapError, AuthorityHistoryError) as exc:
        return {"error": str(exc)}
    return None


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
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
