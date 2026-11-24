#!/usr/bin/env python3
"""Differential oracle for agent/context_compressor_durable.py (w20_contextcom_7fc1af).

The live mixin is imported against stubbed ``persistence.core.errors`` and a
stubbed ``agent.agent_init_session.writer_interface``, then every harness line
is replayed through the real ``_durable_read`` / ``_durable_write`` /
``_load_durable`` / ``_durable_verb`` with a scripted store handle and session
store.  Compared values: the verb table, verb resolution (including the KeyError
a writer handle raises for an unknown method), the never-raise read contract,
the coercion and max(default, value) clamp, write success, and the attribute a
load leaves behind.
"""
import copy
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class PersistenceError(Exception):
    pass


for pkg in ("agent", "persistence", "persistence.core"):
    if pkg not in sys.modules:
        _mk(pkg)
_mk("persistence.core.errors", PersistenceError=PersistenceError)

CURRENT = {"handle": None}
_mk("agent.agent_init_session", writer_interface=lambda persistence, session_db: CURRENT["handle"])

NAME = "w20_contextcom_7fc1af_live"
sp = importlib.util.spec_from_file_location(
    NAME, os.path.join(Q, "agent", "context_compressor_durable.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)

ERRORS = {"TypeError": TypeError, "ValueError": ValueError, "KeyError": KeyError,
          "OSError": OSError, "RuntimeError": RuntimeError, "PersistenceError": PersistenceError}
COERCERS = {"int": int, "float": float, "str": str, "none": lambda value: value}


def _verb(spec):
    def call(session_id, *args):
        err = spec.get("error")
        if err:
            raise ERRORS.get(err, RuntimeError)("scripted failure")
        return copy.deepcopy(spec.get("stored"))
    return call


def _handle(spec):
    handle = types.SimpleNamespace()
    for repo in ("lineage", "sessions", "messages"):
        setattr(handle, repo, types.SimpleNamespace())
    for method, (repo, verb) in live._DURABLE_VERBS.items():
        setattr(getattr(handle, repo), verb, _verb(spec))
    return handle


def _session_db(spec):
    db = types.SimpleNamespace()
    if spec.get("session_db_has_method"):
        setattr(db, spec.get("method") or "", _verb(spec))
    return db


def _bind(r):
    """(obj, session_db, handle) for one request, mirroring the fixture's mode."""
    spec = r
    handle = _handle(spec) if r.get("store") == "interface" else None
    db = _session_db(spec)
    obj = live.DurableStateMixin()
    obj._persistence = handle
    obj._session_db = db
    obj._session_id = r.get("session_id")
    CURRENT["handle"] = handle
    return obj, db, handle


def expected(r):
    f = r.get("func")
    if f == "verb_table":
        return {m: {"repository": repo, "verb": verb}
                for m, (repo, verb) in live._DURABLE_VERBS.items()}
    if f == "resolve_verb":
        obj, db, handle = _bind(r)
        method = r.get("method")
        try:
            got = live._durable_verb(handle, db, method)
        except KeyError:
            return {"raised": "KeyError"}
        if got is None:
            return {"via": "none", "repository": None, "verb": method, "callable": False}
        if handle is not None:
            repo, verb = live._DURABLE_VERBS[method]
            return {"via": "store", "repository": repo, "verb": verb, "callable": True}
        return {"via": "session_db", "repository": None, "verb": method, "callable": True}
    if f == "durable_read":
        obj, db, handle = _bind(r)
        method = r.get("method")
        coerce = COERCERS[r.get("coerce") or "int"]
        try:
            found, value = obj._durable_read(method, "label", coerce, r.get("default", 0),
                                             *(r.get("args") or []))
        except KeyError:
            return {"raised": "KeyError"}
        return {"found": found, "value": value}
    if f == "durable_write":
        obj, db, handle = _bind(r)
        try:
            return {"ok": obj._durable_write(r.get("method"), "label", *(r.get("args") or []))}
        except KeyError:
            return {"raised": "KeyError"}
    if f == "load_durable":
        obj, db, handle = _bind(r)
        attr = "_scripted_attr"
        setattr(obj, attr, r.get("initial"))
        coerce = COERCERS[r.get("coerce") or "int"]
        try:
            obj._load_durable(attr, r.get("method"), "label", coerce, r.get("default", 0),
                              *(r.get("args") or []))
        except KeyError:
            return {"raised": "KeyError"}
        return {"value": getattr(obj, attr)}
    raise AssertionError("unknown func %r" % (f,))


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if not isinstance(r, dict) or r.get("func") is None:
        continue
    cases += 1
    try:
        exp = expected(r)
    except Exception as exc:  # noqa: BLE001
        mism += 1
        print("MISMATCH %s case=%s live-error=%r" % (r.get("func"), r.get("case"), exc),
              file=sys.stderr)
        continue
    got = r.get("out")
    if exp != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), exp, got),
              file=sys.stderr)
print("ORACLE w20_contextcom_7fc1af: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
