#!/usr/bin/env python3
"""Oracle for the persistence/conversation/sessions.py FLOW + leaves port
(src/persistence/conversation_sessions_flow.c).

Drives the LIVE sessions.py defs with SCRIPTED seams: json.loads and the seamed
sibling compositions (parse_model_config, is_automatic_end_reason,
SessionRecord._row_cells / _marker_cells, SessionListEntry._head_cells) are
recording stubs fed the SAME scripts the C harness feeds its universal seam.
Compares the DECISION SEQUENCE (ordered seam events {"call":name,...args} in
"trace"), the return ("ret") and the mutated self ("state", RecentBrowseQuery).

Live defs are AST-extracted (compiled with the PEP 563 annotations future-flag so
annotations never evaluate at def-exec time). Module constants, the vocabulary
enums, the error classes (with their Conflict / TransientStoreError bases pulled
from persistence/core/errors.py) and the dataclass METHODS are extracted; the
heavy SessionRepository protocol and the full dataclass bodies never run. Methods
are driven on SimpleNamespace attribute doubles (enums materialized to real
members); the seamed siblings are bound as recording stubs on the doubles.

Fail-closed: a missing/exhausted script raises RuntimeError "no script for <name>"
(recorded, then propagated); 0 cases or a missing extraction => nonzero exit.
BaseException kinds (SystemExit / KeyboardInterrupt / GeneratorExit /
CancelledError) pass through every except site. Scripted raises round-trip their
spec bare; unscripted live raises use exc_spec(). Comparison is ORDER-SENSITIVE
(canonical JSON dumps without sort_keys) because row-dict key order is semantic.
Summary line: 'ORACLE conversation_sessions_flow: N cases, M mismatches'.

Deterministic subset (documented): ASCII str()/int()/float()/repr() operands;
sanitize_title exercises its BMP invisible/whitespace classes + U+2026/U+FFFD;
integral floats collapse to ints on both sides; SessionId is its str value.
"""
import ast
import asyncio
import builtins
import copy
import json
import os
import re
import sys
import types
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Any, Mapping, Optional, Protocol, Sequence, TypeAlias, runtime_checkable
import __future__

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_HERE = Path(__file__).resolve()
_REPO = _HERE.parents[1]
QUARRY = _REPO.parent / "hermes-agent"
if not (QUARRY / "persistence/conversation/sessions.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "persistence/conversation/sessions.py")
ERRSRC = str(QUARRY / "persistence/core/errors.py")

os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "cs_home"))

FLAG = __future__.annotations.compiler_flag

# harness func name == LIVE qualified name (identity map, kept explicit).
MODULE_FUNCS = (
    "is_automatic_end_reason", "is_explicit_close_reason", "bound_activity_description",
    "normalize_activity_provenance", "workspace_key", "parse_model_config",
    "is_corrupt_model_config", "session_yolo_enabled", "sanitize_title",
)
METHODS = {
    "NewSession.__post_init__": ("NewSession", "__post_init__"),
    "SessionRecord.as_session_row": ("SessionRecord", "as_session_row"),
    "SessionRecord._row_cells": ("SessionRecord", "_row_cells"),
    "SessionRecord._marker_cells": ("SessionRecord", "_marker_cells"),
    "SessionWindow.__post_init__": ("SessionWindow", "__post_init__"),
    "SessionListEntry.as_list_row": ("SessionListEntry", "as_list_row"),
    "SessionListEntry._head_cells": ("SessionListEntry", "_head_cells"),
    "CronRunEntry.as_cron_row": ("CronRunEntry", "as_cron_row"),
    "RecentBrowseQuery.__post_init__": ("RecentBrowseQuery", "__post_init__"),
    "RuntimeLock.as_mapping": ("RuntimeLock", "as_mapping"),
    "ReplaceSessionConfig.__post_init__": ("ReplaceSessionConfig", "__post_init__"),
}
ERROR_CTORS = {
    "TitleConflict.__init__": "TitleConflict",
    "CanonicalTitleLocked.__init__": "CanonicalTitleLocked",
    "SessionExists.__init__": "SessionExists",
}
PY = {n: n for n in MODULE_FUNCS}
PY.update({n: n for n in METHODS})
PY.update({n: n for n in ERROR_CTORS})

ENUM_CLASSES = ("SessionLineageKind", "TitleSource", "SessionOrder", "SessionLifecycleStatus")
ERROR_CLASSES = ("TitleConflict", "CanonicalTitleLocked", "SessionExists")
ERR_BASES = ("PersistenceError", "Conflict", "TransientStoreError")


def _exec_node(node, src, ns):
    exec(compile(ast.Module(body=[node], type_ignores=[]), src, "exec", flags=FLAG), ns)


def extract():
    ns = {
        "__name__": "persistence.conversation.sessions",
        "json": json, "re": re, "os": os,
        "dataclass": dataclass, "field": field, "Enum": Enum,
        "Any": Any, "Mapping": Mapping, "Optional": Optional, "Protocol": Protocol,
        "Sequence": Sequence, "TypeAlias": TypeAlias, "runtime_checkable": runtime_checkable,
        "OwnerRule": object, "SessionId": str, "JsonValue": object,
    }
    mod = types.ModuleType(ns["__name__"])
    mod.__dict__.update(ns)
    sys.modules[ns["__name__"]] = mod
    ns = mod.__dict__

    # stub parent packages so any import resolves to the extracted module
    for pkg in ("persistence", "persistence.conversation", "persistence.core",
                "persistence.authority"):
        m = types.ModuleType(pkg)
        m.__path__ = []
        sys.modules.setdefault(pkg, m)
    sys.modules["persistence"].conversation = sys.modules["persistence.conversation"]
    sys.modules["persistence"].core = sys.modules["persistence.core"]
    sys.modules["persistence"].authority = sys.modules["persistence.authority"]
    sys.modules["persistence.conversation"].sessions = mod

    # error bases from core/errors.py
    esrc = open(ERRSRC, encoding="utf-8").read()
    etree = ast.parse(esrc)
    for node in etree.body:
        if isinstance(node, ast.ClassDef) and node.name in ERR_BASES:
            _exec_node(node, ERRSRC, ns)

    src = open(SRC, encoding="utf-8").read()
    tree = ast.parse(src)

    # module constants (Assign / AnnAssign with plain-Name targets), source order
    for node in tree.body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgt = node.targets[0] if isinstance(node, ast.Assign) else node.target
            if isinstance(tgt, ast.Name):
                _exec_node(node, SRC, ns)

    # vocabulary enums
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in ENUM_CLASSES:
            _exec_node(node, SRC, ns)

    # error classes (bases seeded above)
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name in ERROR_CLASSES:
            _exec_node(node, SRC, ns)

    # module functions
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in MODULE_FUNCS:
            _exec_node(node, SRC, ns)

    # dataclass methods: exec the FunctionDef node directly (AST carries structure,
    # no dedent needed), then rebind under the qualified key to avoid __post_init__
    # collisions. The function's __globals__ stays ns (module constants visible).
    for node in tree.body:
        if isinstance(node, ast.ClassDef):
            for sub in node.body:
                if isinstance(sub, ast.FunctionDef):
                    qual = f"{node.name}.{sub.name}"
                    if qual in METHODS:
                        _exec_node(sub, SRC, ns)
                        ns[qual] = ns[sub.name]
                        if qual != sub.name:
                            del ns[sub.name]

    required = list(MODULE_FUNCS) + list(METHODS) + list(ERROR_CLASSES) + list(ENUM_CLASSES) + list(ERR_BASES)
    required += ["SESSION_ROW_COLUMNS", "COMPACT_EXCLUDED_COLUMNS", "AUTOMATIC_END_REASONS",
                 "BOUNDARY_END_REASONS", "ACTIVITY_PROVENANCES", "MAX_TITLE_LENGTH",
                 "ACTIVITY_DESCRIPTION_MAX", "YOLO_KEY", "_TITLE_SURROGATE_RE",
                 "_TITLE_CONTROL_RE", "_TITLE_INVISIBLE_RE"]
    missing = [n for n in required if n not in ns]
    if missing:
        print("FATAL conversation_sessions_flow: extraction missing names:", sorted(missing))
        sys.exit(2)
    return ns


NS = extract()
REAL_JSON = json


def make_exc(spec):
    t = spec.get("type", "Exception")
    msg = spec.get("message", "")
    if t == "JSONDecodeError":
        return json.JSONDecodeError(msg, "", 0)
    if t == "SystemExit":
        return SystemExit(spec.get("code", msg))
    if t == "KeyboardInterrupt":
        return KeyboardInterrupt(msg)
    if t == "GeneratorExit":
        return GeneratorExit(msg)
    if t == "CancelledError":
        return asyncio.CancelledError(msg)
    if t in ERROR_CLASSES:
        cls = NS[t]
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    cls = getattr(builtins, t, None)
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls(msg)
    return type(t, (Exception,), {})(msg)


def exc_spec(e):
    return {"__raise__": type(e).__name__, "message": str(e)}


def sentinel_from_spec(spec):
    return {"__raise__": spec.get("type", "Exception"), "message": spec.get("message", "")}


class Raise:
    def __init__(self, spec):
        self.spec = spec


class CaseCtx:
    def __init__(self, inp):
        self.inp = inp
        self.trace = []
        self.io = inp.get("io", {}) or {}
        self.cursors = {}
        self.scripted = []

    def script_next(self, name):
        entry = self.io.get(name)
        if entry is None:
            return Raise({"type": "RuntimeError", "message": f"no script for {name}"})
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                return Raise({"type": "RuntimeError", "message": f"no script for {name}"})
            self.cursors[name] = idx + 1
            return entry[idx]
        return entry

    def raise_scripted(self, spec):
        e = make_exc(spec)
        self.scripted.append((e, spec))
        raise e

    def rec(self, name, args):
        ev = {"call": name}
        ev.update(args)
        self.trace.append(ev)
        result = self.script_next(name)
        if isinstance(result, Raise):
            self.raise_scripted(result.spec)
        if isinstance(result, dict):
            if isinstance(result.get("raise"), dict):
                self.raise_scripted(result["raise"])
            if "ret" in result:
                return result["ret"]
        return result

    def recorder(self, name, proj):
        def call(*a, **k):
            return self.rec(name, proj(a, k))
        return call


def canon(v):
    if isinstance(v, BaseException):
        return exc_spec(v)
    if isinstance(v, types.SimpleNamespace):
        return {k: canon(x) for k, x in vars(v).items()}
    if isinstance(v, float) and v.is_integer():
        return int(v)
    if isinstance(v, tuple):
        return [canon(x) for x in v]
    if isinstance(v, list):
        return [canon(x) for x in v]
    if isinstance(v, dict):
        return {k: canon(x) for k, x in v.items()}
    if isinstance(v, Enum):
        return v.value
    return v


def _ns(d):
    return types.SimpleNamespace(**(d or {}))


def _row_double(d):
    """SessionRecord attribute double; materialize title_source enum."""
    d = dict(d or {})
    ts = d.get("title_source")
    if ts is not None:
        d["title_source"] = NS["TitleSource"](ts)
    return types.SimpleNamespace(**d)


def drive(func, inp, ctx, out):
    ns = NS
    saved = {}

    def save(key):
        saved[key] = ns.get(key)

    def restore():
        for k, v in saved.items():
            ns[k] = v

    rec = ctx.rec
    try:
        if func == "is_automatic_end_reason":
            r = ns["is_automatic_end_reason"](inp.get("reason"))
        elif func == "is_explicit_close_reason":
            save("is_automatic_end_reason")
            ns["is_automatic_end_reason"] = ctx.recorder("is_automatic_end_reason",
                                                lambda a, k: {"reason": a[0]})
            r = ns["is_explicit_close_reason"](inp.get("reason"))
        elif func == "bound_activity_description":
            r = ns["bound_activity_description"](inp.get("description"))
        elif func == "normalize_activity_provenance":
            r = ns["normalize_activity_provenance"](inp.get("provenance"))
        elif func == "workspace_key":
            r = ns["workspace_key"](inp.get("row") or {})
        elif func == "parse_model_config":
            save("json")
            ns["json"] = types.SimpleNamespace(
                loads=ctx.recorder("json.loads", lambda a, k: {"s": a[0]}),
                JSONDecodeError=REAL_JSON.JSONDecodeError)
            r = ns["parse_model_config"](inp.get("raw"))
        elif func == "is_corrupt_model_config":
            save("json")
            ns["json"] = types.SimpleNamespace(
                loads=ctx.recorder("json.loads", lambda a, k: {"s": a[0]}),
                JSONDecodeError=REAL_JSON.JSONDecodeError)
            r = ns["is_corrupt_model_config"](inp.get("raw"))
        elif func == "session_yolo_enabled":
            save("parse_model_config")
            ns["parse_model_config"] = ctx.recorder("parse_model_config", lambda a, k: {"raw": a[0]})
            r = ns["session_yolo_enabled"](inp.get("model_config"))
        elif func == "sanitize_title":
            r = ns["sanitize_title"](inp.get("title"))
        elif func == "TitleConflict.__init__":
            e = ns["TitleConflict"](inp.get("title"), inp.get("holder"))
            r = {"class": type(e).__name__, "args": list(e.args), "str": str(e), "attrs": vars(e)}
        elif func == "CanonicalTitleLocked.__init__":
            e = ns["CanonicalTitleLocked"](inp.get("session_id"))
            r = {"class": type(e).__name__, "args": list(e.args), "str": str(e), "attrs": vars(e)}
        elif func == "SessionExists.__init__":
            e = ns["SessionExists"](_ns(inp.get("existing")))
            r = {"class": type(e).__name__, "args": list(e.args), "str": str(e), "attrs": vars(e)}
        elif func == "NewSession.__post_init__":
            d = dict(inp.get("self") or {})
            lk = d.get("lineage_kind")
            if lk is not None:
                d["lineage_kind"] = NS["SessionLineageKind"](lk)
            slf = types.SimpleNamespace(**d)
            r = ns["NewSession.__post_init__"](slf)
        elif func == "SessionRecord._row_cells":
            slf = _row_double(inp.get("self"))
            r = ns["SessionRecord._row_cells"](slf, compact=inp.get("compact", False))
        elif func == "SessionRecord._marker_cells":
            slf = _ns(inp.get("self"))
            r = ns["SessionRecord._marker_cells"](slf)
        elif func == "SessionRecord.as_session_row":
            slf = types.SimpleNamespace()
            slf._row_cells = ctx.recorder("SessionRecord._row_cells",
                                 lambda a, k: {"compact": k.get("compact", False)})
            slf._marker_cells = ctx.recorder("SessionRecord._marker_cells", lambda a, k: {})
            r = ns["SessionRecord.as_session_row"](slf)
        elif func == "SessionWindow.__post_init__":
            slf = _ns(inp.get("self"))
            r = ns["SessionWindow.__post_init__"](slf)
        elif func == "SessionListEntry._head_cells":
            session = types.SimpleNamespace()
            session._row_cells = ctx.recorder("SessionRecord._row_cells",
                                     lambda a, k: {"compact": k.get("compact", False)})
            session._marker_cells = ctx.recorder("SessionRecord._marker_cells", lambda a, k: {})
            d = dict(inp.get("self") or {})
            d["session"] = session
            slf = types.SimpleNamespace(**d)
            extra = tuple(tuple(p) for p in (inp.get("extra") or []))
            r = ns["SessionListEntry._head_cells"](slf, *extra)
        elif func == "SessionListEntry.as_list_row":
            d = dict(inp.get("self") or {})
            if isinstance(d.get("lineage_ids"), list):
                d["lineage_ids"] = tuple(d["lineage_ids"])
            slf = types.SimpleNamespace(**d)
            slf._head_cells = ctx.recorder("SessionListEntry._head_cells",
                                  lambda a, k: {"extra": [list(p) for p in a]})
            r = ns["SessionListEntry.as_list_row"](slf)
        elif func == "CronRunEntry.as_cron_row":
            entry = types.SimpleNamespace()
            entry._head_cells = ctx.recorder("SessionListEntry._head_cells",
                                    lambda a, k: {"extra": [list(p) for p in a]})
            d = dict(inp.get("self") or {})
            d["entry"] = entry
            slf = types.SimpleNamespace(**d)
            r = ns["CronRunEntry.as_cron_row"](slf)
        elif func == "RecentBrowseQuery.__post_init__":
            slf = _ns(inp.get("self"))
            try:
                r = ns["RecentBrowseQuery.__post_init__"](slf)
            finally:
                out["state"] = {"self": vars(slf)}
        elif func == "RuntimeLock.as_mapping":
            slf = _ns(inp.get("self"))
            r = ns["RuntimeLock.as_mapping"](slf)
        elif func == "ReplaceSessionConfig.__post_init__":
            slf = _ns(inp.get("self"))
            r = ns["ReplaceSessionConfig.__post_init__"](slf)
        else:
            raise KeyError(func)
        return r
    finally:
        restore()


def run_case(func, inp):
    ctx = CaseCtx(inp)
    out = {"ret": None, "trace": ctx.trace, "state": {}}
    try:
        r = drive(func, inp, ctx, out)
        out["ret"] = canon(r)
    except BaseException as e:
        for (se, spec) in ctx.scripted:
            if e is se:
                out["ret"] = sentinel_from_spec(spec)
                break
        else:
            out["ret"] = exc_spec(e)
        out["ret"] = canon(out["ret"])
    out["state"] = canon(out.get("state") or {})
    return out


def dumps_ordered(v):
    return json.dumps(canon(v), ensure_ascii=False, separators=(",", ":"), default=str)


def main():
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            c = json.loads(line)
        except json.JSONDecodeError:
            continue
        cases += 1
        func = c.get("func", "")
        if func not in PY:
            mism += 1
            print(f"PYERR {func}: unknown func")
            continue
        try:
            want = run_case(func, c.get("in") or {})
        except Exception as e:
            mism += 1
            print(f"PYERR {func}: {type(e).__name__}: {e}")
            continue
        got = c.get("out") or {}
        ok = (dumps_ordered(got.get("trace")) == dumps_ordered(want.get("trace"))
              and dumps_ordered(got.get("ret")) == dumps_ordered(want.get("ret"))
              and dumps_ordered(got.get("state")) == dumps_ordered(want.get("state")))
        if not ok:
            mism += 1
            print(f"MISMATCH {func}")
            print(f"  C  ={dumps_ordered(got)}")
            print(f"  PY ={dumps_ordered(want)}")
    print(f"ORACLE conversation_sessions_flow: {cases} cases, {mism} mismatches")
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())