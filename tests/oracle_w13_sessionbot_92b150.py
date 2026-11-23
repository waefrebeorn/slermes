#!/usr/bin/env python3
"""Differential oracle for gate w13_sessionbot_92b150 (gateway/session_bot_cancel.py).

The LIVE _cancel is driven with scripted collaborators: delivery-id validation,
receipt store, admission row, connection.cancel answer and receipt result. Only
those collaborators are stubbed; the control flow under test is the quarry code.
"""
import asyncio
import contextlib
import copy
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w13_sessionbot_92b150"
sys.path.insert(0, QUARRY)
NS = types.SimpleNamespace
CTX = {}


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


def SessionRef(*a, **k):
    return ("ref", a, tuple(sorted((k or {}).items())))


_seed("gateway.session_contract", SessionRef=SessionRef)


class _Admissions:
    def get(self, admission_id):
        st = CTX.get("row_status")
        return NS(status=st) if st is not None else None


def authority_persistence(authority):
    return NS(admissions=_Admissions())


_seed("gateway.session_persistence", authority_persistence=authority_persistence)
_seed("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)


def _delivery_id(v):
    if not isinstance(v, str) or not v:
        raise ValueError("bad delivery id")
    return v


def _delivery_result(authority, record):
    record["status"] = CTX.get("receipt_status")
    return {}


def _home(authority, actor, profile):
    return "home"


def _retry_admission(authority, record):
    return CTX.get("retry_id")


def _retry_eligible(authority, record):
    return bool(CTX.get("retry_eligible"))


def _retry_identity(key):
    return "ident:" + str(key)


_seed("gateway.session_bot", _delivery_id=_delivery_id, _delivery_result=_delivery_result,
      _home=_home, _retry_admission=_retry_admission, _retry_eligible=_retry_eligible,
      _retry_identity=_retry_identity)
_seed("gateway.config", Platform=NS(LOCAL="local"))


@contextlib.asynccontextmanager
async def mailbox_lock(authority):
    yield None


async def read_receipt(home, key):
    return (None, copy.deepcopy(CTX.get("record")))


async def write_receipt(home, record):
    return None


def remember_receipt(authority, home, record):
    return None


_seed("gateway.session_bot_mailbox", mailbox_lock=mailbox_lock, read_receipt=read_receipt,
      write_receipt=write_receipt, remember_receipt=remember_receipt)
_seed("gateway.session_authorities", require_serving=lambda a: None)
_seed("gateway.session_runtime_workers", track_mutation=lambda a, c: c)

SRC = QUARRY + "/gateway/session_bot_cancel.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)


class Conn:
    def __init__(self, actor):
        self.authority = NS(profile_id="p")
        self.actor = actor

    async def cancel(self, ref, args):
        return {"status": CTX.get("answer_status"), "outcome": CTX.get("answer_outcome")}


def compute(r):
    CTX.clear()
    CTX.update({
        "retry_id": r.get("retry_id"),
        "row_status": r.get("row_status"),
        "retry_eligible": r.get("retry_eligible"),
        "answer_status": r.get("answer_status"),
        "answer_outcome": r.get("answer_outcome"),
        "receipt_status": r.get("receipt_status"),
        "record": copy.deepcopy(r.get("record")),
    })
    conn = Conn(NS(subject=r.get("actor_subject")))
    try:
        return asyncio.run(live._cancel(conn, r.get("params") or {}))
    except AuthorityRefusal as exc:
        return {"error": exc.reason}


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
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
