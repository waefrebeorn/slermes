#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_auto_maintenance.py
(gate w20_cliautomai_c467ab).

The live module is extracted with ast.parse/exec (it only imports logging and
sqlite3). Its three live collaborators are scripted: the session-db handle is a
recording fake built from the fixture, and load_config / get_hermes_home are
stubbed module attributes, exactly the seams the module imports them through.
The compared value is the ordered transcript: each db call with its kwargs and
each formatted log record.
"""
import ast
import json
import logging
import os
import sqlite3
import sys
import types
import __future__
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

SRC = Q + "/hermes_cli/cli_auto_maintenance.py"
GATE = "w20_cliautomai_c467ab"
_ANN = __future__.annotations.compiler_flag

ERRORS = {
    "ValueError": ValueError, "RuntimeError": RuntimeError, "TypeError": TypeError,
    "OSError": OSError, "KeyError": KeyError, "AttributeError": AttributeError,
    "sqlite3.Error": sqlite3.Error, "sqlite3.OperationalError": sqlite3.OperationalError,
}


def _mk(dotted, **attrs):
    parts = dotted.split(".")
    for i in range(1, len(parts) + 1):
        dn = ".".join(parts[:i])
        if dn not in sys.modules or sys.modules[dn] is None:
            m = types.ModuleType(dn)
            m.__path__ = []
            sys.modules[dn] = m
    m = sys.modules[dotted]
    for k, v in attrs.items():
        setattr(m, k, v)
    return m


CUR = {"config": {"sessions": {}}, "home": "", "events": []}


def load_config():
    CUR["events"].append({"call": "load_config", "kwargs": {}})
    cfg = CUR["config"]
    if isinstance(cfg, dict) and isinstance(cfg.get("__raise__"), dict):
        err = cfg["__raise__"]
        cls = ERRORS.get(err.get("type", "RuntimeError"), RuntimeError)
        raise cls(err.get("message", ""))
    return cfg


def get_hermes_home():
    return Path(CUR["home"] or "")


_mk("hermes_cli")
_mk("hermes_cli.config", load_config=load_config)
_mk("hermes_constants", get_hermes_home=get_hermes_home)

G = {"__name__": "cli_auto_maint_live", "__builtins__": __builtins__,
     "logging": logging, "sqlite3": sqlite3, "logger": logging.getLogger("cli")}


def extract():
    tree = ast.parse(open(SRC, encoding="utf-8").read())
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef,
                             ast.Assign, ast.AnnAssign)):
            mod = ast.Module(body=[node], type_ignores=[])
            exec(compile(mod, SRC, "exec", flags=_ANN, dont_inherit=True), G)


extract()
RUN = G["_run_state_db_auto_maintenance"]


def _posix(v):
    return str(v).replace("\\", "/") if v is not None else None


class FakeDB:
    def __init__(self, spec, events):
        spec = spec if isinstance(spec, dict) else {}
        self.meta = spec.get("meta") if isinstance(spec.get("meta"), dict) else {}
        self.repairs = spec.get("repairs") if isinstance(spec.get("repairs"), dict) else {}
        self.calls = spec.get("calls") if isinstance(spec.get("calls"), dict) else {}
        self.events = events

    def _raise_for(self, slot):
        if isinstance(slot, dict) and isinstance(slot.get("__raise__"), dict):
            err = slot["__raise__"]
            cls = ERRORS.get(err.get("type", "RuntimeError"), RuntimeError)
            raise cls(err.get("message", ""))

    def get_meta(self, key):
        self.events.append({"call": "get_meta", "args": [key]})
        return self.meta.get(key)

    def set_meta(self, key, value):
        self.events.append({"call": "set_meta", "args": [key, value]})

    def _repair(self, key):
        slot = self.repairs.get(key)
        self._raise_for(slot)
        return (slot or {}).get("count", 0) if isinstance(slot, dict) else 0

    def prune_empty_ghost_sessions(self, sessions_dir=None):
        self.events.append({"call": "prune_empty_ghost_sessions",
                            "kwargs": {"sessions_dir": _posix(sessions_dir)}})
        return self._repair("ghost_session_prune_v1")

    def finalize_orphaned_compression_sessions(self):
        self.events.append({"call": "finalize_orphaned_compression_sessions", "kwargs": {}})
        return self._repair("orphaned_compression_finalize_v1")

    def maybe_auto_archive(self, idle_days=3.0, min_interval_hours=24):
        return self._hook("maybe_auto_archive",
                          {"idle_days": idle_days, "min_interval_hours": min_interval_hours})

    def maybe_auto_prune_and_vacuum(self, retention_days=90, min_interval_hours=24,
                                    min_vacuum_interval_days=30, vacuum=True,
                                    sessions_dir=None):
        return self._hook("maybe_auto_prune_and_vacuum", {
            "retention_days": retention_days, "min_interval_hours": min_interval_hours,
            "min_vacuum_interval_days": min_vacuum_interval_days, "vacuum": vacuum,
            "sessions_dir": _posix(sessions_dir)})

    def _hook(self, name, kwargs):
        self._raise_for(self.calls.get(name))
        self.events.append({"call": name, "kwargs": kwargs})
        return None


class Capture(logging.Handler):
    def __init__(self, events):
        super().__init__(logging.DEBUG)
        self.events = events

    def emit(self, record):
        self.events.append({"log": record.levelname.lower(), "msg": record.getMessage()})


def run_case(r):
    events = []
    CUR["events"] = events
    lg = logging.getLogger("cli")
    lg.setLevel(logging.DEBUG)
    lg.propagate = False
    for h in list(lg.handlers):
        lg.removeHandler(h)
    lg.addHandler(Capture(events))
    try:
        CUR["config"] = r.get("config") if r.get("config") is not None else {"sessions": {}}
        CUR["home"] = r["home"] if isinstance(r.get("home"), str) else (
            os.environ.get("SLERMES_HOME") or os.environ.get("HOME") or "")
        db = r.get("db")
        db_obj = None if db is None else (FakeDB(db, events) if isinstance(db, dict) else db)
        RUN(db_obj)
    finally:
        for h in list(lg.handlers):
            lg.removeHandler(h)
    return events


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
        exp = run_case(r)
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
print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
sys.exit(1 if mism else 0)
