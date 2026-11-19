#!/usr/bin/env python3
"""Oracle for the gateway/session.py FLOW port
(src/gateway/gateway_session_flow.c).

Drives the LIVE gateway/session.py decision chains with SCRIPTED seams: every
claimed sibling (is_shared_multi_user_session / _slack_tools_loaded /
_discord_tools_loaded), persistence mixin verb (_entry_locked /
_ensure_loaded_locked / _save / _persistence_for_key / _reopen_session_row /
_announce_route / _heal_compression_tip_locked / _query_recoverable_session /
_session_create_kwargs), identity helper (canonical_whatsapp_identifier /
transport_profile_of / tag_safe_display_name), lifecycle _new_session_id,
platform_registry lookup and logger call is a recording stub fed the same
scripts the C harness feeds its universal seam. Compares the DECISION SEQUENCE
(ordered seam events {"name","args","ret"} in "trace") plus the final return
("ret") and the mutated self/world state ("state").

The live defs are AST-extracted from gateway/session.py (module constants seeded
twice, Assign AND AnnAssign, plain-Name targets only; the SessionSource /
SessionContext / SessionEntry / _RouteChecks / _RouteDecision / SessionStore
ClassDefs exec whole with stub mixin bases). Platform is AST-extracted from
gateway/config.py. Every exec namespace is a registered sys.modules module
(py3.14 dataclasses _is_type needs cls.__module__ resolvable). The heavy quarry
import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE gateway_session_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
- SessionSource / SessionEntry / _RouteDecision travel as FULL field dicts
  (dataclass defaults filled on both sides); Platform as .value strings;
  datetime fields as canonical round-tripping ISO strings; _rows_pending as a
  SORTED id array; origin a nested full SessionSource dict.
- _entry_locked returns the STORED _entries[key] object (live identity); the
  script gates null/raise and supplies the trace ret.
- _update_entry is modeled by its contract (resolve via _entry_locked, run the
  mutate callback, full-save via _save only when changed).
- logger calls are seams recording {msg, args} verbatim (exc slot -> str(exc)).
- Fixture corpus is ASCII; scripted raise kinds are plain-message classes
  (KeyError excluded: str(KeyError(k)) == repr(k)).
"""
import ast
import json
import os
import re
import sys
import textwrap
import types
from dataclasses import dataclass, field, fields, asdict, is_dataclass, MISSING
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Any, Optional, Dict, List
import __future__ as _fut

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "gateway", "session.py")
CFG = os.path.join(Q, "gateway", "config.py")
_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    exec(compile(textwrap.dedent(seg(src, node)), path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


def seed_constants(src, ns, path):
    for node in ast.parse(src).body:
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            tgts = node.targets if isinstance(node, ast.Assign) else [node.target]
            if not all(isinstance(t, ast.Name) for t in tgts):
                continue
            try:
                exec_node(src, node, ns, path)
            except Exception:
                pass


def class_nodes(src, names):
    return [n for n in ast.parse(src).body if isinstance(n, ast.ClassDef) and n.name in names]


def func_nodes(src):
    return [n for n in ast.parse(src).body
            if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef))]


# ── session.py namespace ──
def extract_ns():
    mod = new_module("gse_oracle_sess")
    ns = mod.__dict__
    import threading
    ns.update({"dataclass": dataclass, "field": field, "fields": fields,
               "Any": Any, "Optional": Optional, "Dict": Dict, "List": List,
               "datetime": datetime, "timedelta": __import__("datetime").timedelta,
               "Enum": Enum, "threading": threading, "json": json,
               "SimpleNamespace": types.SimpleNamespace})
    cfg = open(CFG, encoding="utf-8").read()
    for node in class_nodes(cfg, ["Platform"]):
        exec_node(cfg, node, ns, CFG)
    ns["HomeChannel"] = type("HomeChannel", (), {})
    ns["GatewayConfig"] = type("GatewayConfig", (), {})
    for base in ("SessionPersistenceMixin", "SessionRecoveryMixin", "SessionLifecycleMixin",
                 "SessionTranscriptMixin", "SessionPromptPinMixin"):
        ns[base] = type(base, (), {})
    ns["_DB_UNPINNED"] = object()
    src = open(SRC, encoding="utf-8").read()
    seed_constants(src, ns, SRC)
    for node in func_nodes(src):
        exec_node(src, node, ns, SRC)
    for node in class_nodes(src, ["SessionSource", "SessionContext", "SessionEntry",
                                  "_RouteChecks", "_RouteDecision", "_SessionFlight",
                                  "SessionStore"]):
        exec_node(src, node, ns, SRC)
    seed_constants(src, ns, SRC)
    required = ["SessionSource", "SessionContext", "SessionEntry", "_RouteChecks",
                "_RouteDecision", "SessionStore", "Platform", "_PII_SAFE_PLATFORMS",
                "_SLACK_TOOLS_NOTE", "_SLACK_NO_TOOLS_NOTE", "_CHAT_TYPE_PREFIX"]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_gateway_session_flow: missing {missing}")
        sys.exit(2)
    for meth in PORTED:
        if meth in MODULE_FUNCS:
            if meth not in ns:
                print(f"FATAL oracle_gateway_session_flow: module missing {meth}")
                sys.exit(2)
        elif meth == "_describe":
            if not hasattr(ns["SessionSource"], "_describe"):
                print("FATAL oracle_gateway_session_flow: SessionSource missing _describe")
                sys.exit(2)
        elif meth == "schedule_reset":
            if not hasattr(ns["_RouteDecision"], "schedule_reset"):
                print("FATAL oracle_gateway_session_flow: _RouteDecision missing schedule_reset")
                sys.exit(2)
        elif not hasattr(ns["SessionStore"], meth):
            print(f"FATAL oracle_gateway_session_flow: SessionStore missing {meth}")
            sys.exit(2)
    return ns


PORTED = ["_is_path_unsafe", "_session_key_namespace", "_describe", "_canonical_participant",
          "_should_redact_pii", "label_shared_sender", "_slack_platform_notes",
          "_discord_platform_notes", "schedule_reset", "_lazy", "_has_active_processes_safe",
          "lookup_by_session_key", "set_session_yolo", "release_route", "purge_profile_routing",
          "rekey_profile_routing", "remove_by_session_id", "restore_route",
          "_replace_route_locked", "_route_recover", "_route_create", "_apply_route_checks"]
MODULE_FUNCS = {"_is_path_unsafe", "_session_key_namespace", "_canonical_participant",
                "_should_redact_pii", "label_shared_sender", "_slack_platform_notes",
                "_discord_platform_notes"}

NS = extract_ns()
SessionSource = NS["SessionSource"]
SessionEntry = NS["SessionEntry"]
SessionContext = NS["SessionContext"]
RouteChecks = NS["_RouteChecks"]
RouteDecision = NS["_RouteDecision"]
SessionStore = NS["SessionStore"]
Platform = NS["Platform"]


# ── canonical projection ──
def canon(v):
    def conv(x):
        if is_dataclass(x) and not isinstance(x, type):
            return conv(asdict(x))
        if isinstance(x, Enum):
            return x.value
        if isinstance(x, datetime):
            return x.isoformat()
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
            return sorted((conv(i) for i in x), key=lambda z: json.dumps(z, sort_keys=True))
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


def proj_source(s):
    return canon(asdict(s)) if s is not None else None


def proj_entry(e):
    return canon(asdict(e)) if e is not None else None


# ── exception materialization / projection ──
EXC_CLASSES = {
    "OSError": OSError, "TimeoutError": TimeoutError, "PermissionError": PermissionError,
    "FileNotFoundError": FileNotFoundError, "ValueError": ValueError, "TypeError": TypeError,
    "RuntimeError": RuntimeError, "AttributeError": AttributeError, "ImportError": ImportError,
    "Exception": Exception, "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors the C gse_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                self._record_raise(name, args, RuntimeError(f"script exhausted for {name}"))
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                self._record_raise(name, args, make_exc(used["raise"]))
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        return ret


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


class RecLogger:
    def __init__(self, ctx):
        self._ctx = ctx

    def _log(self, level, msg, *args):
        proj = [str(a) if isinstance(a, BaseException) else a for a in args]
        return self._ctx.call(f"logger.{level}", {"msg": msg, "args": proj})

    def warning(self, msg, *a):
        return self._log("warning", msg, *a)

    def info(self, msg, *a):
        return self._log("info", msg, *a)

    def debug(self, msg, *a):
        return self._log("debug", msg, *a)


# ── materialization (fixture dict -> real dataclass instance) ──
def _defaults(cls):
    out = {}
    for f in fields(cls):
        if f.default is not MISSING:
            out[f.name] = f.default
        elif f.default_factory is not MISSING:
            out[f.name] = f.default_factory()
    return out


def make_source(d):
    if d is None:
        return None
    kw = _defaults(SessionSource)
    kw.update({k: v for k, v in d.items()})
    kw["platform"] = Platform(d["platform"])
    kw["chat_id"] = str(d["chat_id"])
    return SessionSource(**kw)


def _dt(v):
    return datetime.fromisoformat(v) if isinstance(v, str) and v else None


def make_entry(d):
    if d is None:
        return None
    kw = _defaults(SessionEntry)
    kw.update({k: v for k, v in d.items()})
    kw["session_key"] = d["session_key"]
    kw["session_id"] = d["session_id"]
    kw["created_at"] = datetime.fromisoformat(d["created_at"])
    kw["updated_at"] = datetime.fromisoformat(d["updated_at"])
    kw["origin"] = make_source(d["origin"]) if isinstance(d.get("origin"), dict) else None
    kw["platform"] = Platform(d["platform"]) if d.get("platform") else None
    kw["metadata"] = dict(d["metadata"]) if isinstance(d.get("metadata"), dict) else {}
    kw["last_resume_marked_at"] = _dt(d.get("last_resume_marked_at"))
    kw["active_turn_started_at"] = _dt(d.get("active_turn_started_at"))
    return SessionEntry(**kw)


def make_decision(d):
    kw = _defaults(RouteDecision)
    kw.update({k: v for k, v in (d or {}).items()})
    kw["entry"] = make_entry(d["entry"]) if isinstance((d or {}).get("entry"), dict) else None
    return RouteDecision(**kw)


def make_checks(d):
    if d is None:
        return None
    return RouteChecks(session_id=d["session_id"], canonical_id=d["canonical_id"],
                       is_stale=d["is_stale"], reset_reason=d["reset_reason"])


def make_context(d):
    d = d or {}
    return SessionContext(source=make_source(d.get("source")), connected_platforms=[],
                          home_channels={},
                          shared_multi_user_session=d.get("shared_multi_user_session", False))


# ── self double (attributes backed by the state dict; seam methods are instance
#    attrs so they never pollute out.state) ──
class SelfDouble:
    def __init__(self, d):
        object.__setattr__(self, "_d", d)

    def __getattr__(self, k):
        d = object.__getattribute__(self, "_d")
        if k in d:
            return d[k]
        raise AttributeError("'types.SimpleNamespace' object has no attribute '%s'" % k)

    def __setattr__(self, k, v):
        object.__getattribute__(self, "_d")[k] = v


class NoopCM:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


def stub_modules(mapping):
    for dotted, attrs in mapping.items():
        parts = dotted.split(".")
        for i in range(1, len(parts) + 1):
            name = ".".join(parts[:i])
            if name not in sys.modules:
                m = types.ModuleType(name)
                m.__path__ = []
                sys.modules[name] = m
            if i > 1:
                setattr(sys.modules[".".join(parts[:i - 1])], parts[i - 1], sys.modules[name])
        mod = sys.modules[dotted]
        for k, v in attrs.items():
            setattr(mod, k, v)


def setup_store(state, ctx):
    d = dict(state)
    # Mirror gse_state_setup: materialize ONLY the store keys present in the
    # fixture state (leaf cases carry {} and must echo {}).
    if "_entries" in d:
        d["_entries"] = {k: make_entry(v) for k, v in (d["_entries"] or {}).items()}
    if "decision" in d:
        d["decision"] = make_decision(d["decision"])
    if "_rows_pending" in d:
        d["_rows_pending"] = set(d["_rows_pending"] or [])
    if "_session_owner_hints" in d:
        d["_session_owner_hints"] = dict(d["_session_owner_hints"] or {})
    entries = d.get("_entries", {})
    slf = SelfDouble(d)
    object.__setattr__(slf, "_lock", NoopCM())

    def entry_locked(session_key):
        raw = ctx.call("_entry_locked", {"session_key": session_key})
        if raw is None:
            return None
        return entries.get(session_key)   # stored object (live identity)

    def update_entry(session_key, mutate):
        entry = entry_locked(session_key)
        if entry is None or mutate(entry) is False:
            return False
        ctx.call("_save", {})
        return True

    object.__setattr__(slf, "_entry_locked", entry_locked)
    object.__setattr__(slf, "_update_entry", update_entry)
    object.__setattr__(slf, "_ensure_loaded_locked", rec(ctx, "_ensure_loaded_locked", lambda: {}))
    object.__setattr__(slf, "_save", rec(ctx, "_save", lambda: {}))
    object.__setattr__(slf, "_persistence_for_key",
                       rec(ctx, "_persistence_for_key", lambda session_key: {"session_key": session_key}))
    object.__setattr__(slf, "_reopen_session_row",
                       rec(ctx, "_reopen_session_row",
                           lambda session_key, session_id, *, log_prefix="":
                           {"session_key": session_key, "session_id": session_id, "log_prefix": log_prefix}))
    object.__setattr__(slf, "_announce_route",
                       rec(ctx, "_announce_route",
                           lambda session_key, entry: {"session_key": session_key, "entry": proj_entry(entry)}))
    object.__setattr__(slf, "_heal_compression_tip_locked",
                       rec(ctx, "_heal_compression_tip_locked",
                           lambda entry, original_session_id, canonical_session_id:
                           {"entry": proj_entry(entry), "original_session_id": original_session_id,
                            "canonical_session_id": canonical_session_id}))
    object.__setattr__(slf, "_query_recoverable_session",
                       rec(ctx, "_query_recoverable_session",
                           lambda *, session_key, source, now:
                           {"session_key": session_key, "source": proj_source(source), "now": now.isoformat()},
                           build=lambda r: make_entry(r) if isinstance(r, dict) else None))
    object.__setattr__(slf, "_session_create_kwargs",
                       rec(ctx, "_session_create_kwargs",
                           lambda *, session_id, session_key, origin, source_value, display_name, parent_session_id:
                           {"session_id": session_id, "session_key": session_key, "origin": proj_source(origin),
                            "source_value": source_value, "display_name": display_name,
                            "parent_session_id": parent_session_id}))
    if state.get("_has_active_processes_fn") is not None:
        object.__setattr__(slf, "_has_active_processes_fn",
                           rec(ctx, "_has_active_processes_fn", lambda session_key: {"session_key": session_key}))
    # Bind the ported SessionStore methods onto the double (instance attrs, so
    # they never pollute out.state). self.<collaborator> resolves to the seams above.
    for meth in PORTED:
        if meth in MODULE_FUNCS or meth in ("_describe", "schedule_reset"):
            continue
        object.__setattr__(slf, meth, types.MethodType(getattr(SessionStore, meth), slf))
    return slf, d


def install_globals(ctx):
    NS["canonical_whatsapp_identifier"] = rec(ctx, "canonical_whatsapp_identifier",
                                              lambda value: {"value": value})
    NS["transport_profile_of"] = rec(ctx, "transport_profile_of",
                                     lambda source: {"source": proj_source(source)})
    NS["_new_session_id"] = rec(ctx, "_new_session_id", lambda now: {"now": now.isoformat()})
    NS["is_shared_multi_user_session"] = rec(
        ctx, "is_shared_multi_user_session",
        lambda source, *, group_sessions_per_user=True, thread_sessions_per_user=False:
        {"source": proj_source(source), "group_sessions_per_user": group_sessions_per_user,
         "thread_sessions_per_user": thread_sessions_per_user})
    NS["_slack_tools_loaded"] = rec(ctx, "_slack_tools_loaded", lambda: {})
    NS["_discord_tools_loaded"] = rec(ctx, "_discord_tools_loaded", lambda: {})
    NS["logger"] = RecLogger(ctx)
    stub_modules({
        "gateway.platform_registry": {
            "platform_registry": types.SimpleNamespace(
                get=rec(ctx, "platform_registry.get", lambda platform: {"platform": platform},
                        build=lambda r: types.SimpleNamespace(**r) if isinstance(r, dict) else r))},
        "gateway.platforms.helpers": {
            "tag_safe_display_name": rec(ctx, "tag_safe_display_name", lambda name: {"name": name})},
    })


def invoke(func, args, slf, state, ctx):
    if func == "_is_path_unsafe":
        return NS["_is_path_unsafe"](args.get("value"), strict=args.get("strict", True))
    if func == "_session_key_namespace":
        return NS["_session_key_namespace"](args.get("profile"))
    if func == "_describe":
        return SessionSource._describe(args["chat_type"], args["user_label"], args["chat_label"])
    if func == "_canonical_participant":
        return NS["_canonical_participant"](make_source(args.get("source")))
    if func == "_should_redact_pii":
        return NS["_should_redact_pii"](Platform(args["platform"]), args.get("enabled", False))
    if func == "label_shared_sender":
        return NS["label_shared_sender"](
            make_source(args.get("source")), args.get("text"),
            group_sessions_per_user=args.get("group_sessions_per_user", True),
            thread_sessions_per_user=args.get("thread_sessions_per_user", False))
    if func == "_slack_platform_notes":
        return NS["_slack_platform_notes"](make_context(args.get("context")))
    if func == "_discord_platform_notes":
        return NS["_discord_platform_notes"](make_context(args.get("context")))
    if func == "schedule_reset":
        dec = state["decision"]
        return dec.schedule_reset(args["reason"], make_entry(args["ended"]), args.get("had_activity", False))
    if func == "_lazy":
        return slf._lazy(args["name"], rec(ctx, "factory", lambda: {}))
    if func == "_has_active_processes_safe":
        return slf._has_active_processes_safe(args["session_key"], context=args["context"])
    if func == "lookup_by_session_key":
        return slf.lookup_by_session_key(args["session_key"])
    if func == "set_session_yolo":
        return slf.set_session_yolo(args["session_key"], args.get("enabled"))
    if func == "release_route":
        return slf.release_route(args["session_key"], args["session_id"])
    if func == "purge_profile_routing":
        return slf.purge_profile_routing(args.get("profile"))
    if func == "rekey_profile_routing":
        return slf.rekey_profile_routing(args.get("old_name"), args.get("new_name"))
    if func == "remove_by_session_id":
        return slf.remove_by_session_id(args.get("session_id"))
    if func == "restore_route":
        return slf.restore_route(make_entry(args["entry"]), expected_session_id=args["expected_session_id"])
    if func == "_replace_route_locked":
        return slf._replace_route_locked(args["session_key"], make_entry(args["old_entry"]),
                                         args["session_id"], datetime.fromisoformat(args["now"]),
                                         **(args.get("fields") or {}))
    if func == "_route_recover":
        return slf._route_recover(state["decision"], args["session_key"],
                                  make_source(args.get("source")), datetime.fromisoformat(args["now"]))
    if func == "_route_create":
        observed = None
        if args.get("observed") is not None:
            cur = state["_entries"].get(args["session_key"])
            if args.get("observed_same") and cur is not None:
                observed = cur
            else:
                observed = make_entry(args["observed"])
        return slf._route_create(state["decision"], args["session_key"], make_source(args.get("source")),
                                 datetime.fromisoformat(args["now"]), args.get("force_new", False), observed)
    if func == "_apply_route_checks":
        return slf._apply_route_checks(args["session_key"], make_checks(args.get("checks")),
                                       args.get("force_new", False), args.get("touch_activity", True),
                                       datetime.fromisoformat(args["now"]), make_source(args.get("source")))
    raise RuntimeError(f"unknown func {func}")


def drive(func, inp):
    args = inp.get("args") or {}
    state = canon(inp.get("state") or {})
    ctx = Ctx(inp.get("io"))
    install_globals(ctx)
    slf, d = setup_store(state, ctx)
    try:
        ret = invoke(func, args, slf, d, ctx)
    except BaseException as exc:
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(d)}


# ── three-way seam-name cross-check ──
SEAM_NAMES = [
    "canonical_whatsapp_identifier", "platform_registry.get", "is_shared_multi_user_session",
    "tag_safe_display_name", "_slack_tools_loaded", "_discord_tools_loaded", "factory",
    "_has_active_processes_fn", "logger.warning", "logger.info",
    "_entry_locked", "_ensure_loaded_locked", "_save", "_persistence_for_key",
    "_reopen_session_row", "_announce_route", "_heal_compression_tip_locked",
    "transport_profile_of", "_new_session_id", "_query_recoverable_session",
    "_session_create_kwargs",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "gateway_session_flow.c", encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_gateway_session_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*ctx,\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'ctx\.call\(\s*"([^"]+)"', osrc))
    used |= {"logger.warning", "logger.info"}  # RecLogger f"logger.{level}"
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_gateway_session_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "gateway_session_flow.jsonl"
    if fix.exists():
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_gateway_session_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
    fix_funcs = set()
    if fix.exists():
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if line:
                fix_funcs.add(json.loads(line).get("func"))
    badf = fix_funcs - set(PORTED)
    if badf:
        print(f"FATAL oracle_gateway_session_flow: fixture funcs not ported: {sorted(badf)}")
        sys.exit(2)


def first_trace_diff(want, got):
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            return i, w, g
    return None, None, None


def report_mismatch(func, want, got, shown):
    if shown >= 12:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:400]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")
        for k in sorted(keys)[:3]:
            print(f"    {k} want={json.dumps(ws.get(k), ensure_ascii=False)[:200]}")
            print(f"    {k} got ={json.dumps(gs.get(k), ensure_ascii=False)[:200]}")


def main():
    cross_check()
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec.get("func", "")
        inp = rec.get("in") or {}
        got = rec.get("out")
        if not isinstance(got, dict) or "error" in got:
            m += 1
            print(f"MISMATCH {func}: harness emitted no result: {got}")
            continue
        try:
            want = drive(func, inp)
        except BaseException as exc:
            m += 1
            print(f"MISMATCH {func}: oracle drive raised {type(exc).__name__}: {exc}")
            continue
        g = canon(got)
        if g != want:
            m += 1
            report_mismatch(func, want, g, shown)
            shown += 1
    print(f"ORACLE gateway_session_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


if __name__ == "__main__":
    main()