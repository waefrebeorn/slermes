#!/usr/bin/env python3
"""oracle_rest_revive4.py — differential oracle for the rest group-4 revival.

Reads the harness JSONL on stdin ({"func","in","out"}), AST-extracts the LIVE
quarry defs, seeds module constants (Assign + AnnAssign, re-seeded after
classes/functions so partial aliases resolve), drives each live function
under the SAME scripted seam results the C harness consumed, and compares
ordered trace + ret + post-state. BaseException is caught per case
(SystemExit included); missing/exhausted scripts fail closed on both sides.

Seam conventions (identical on both sides; see the port headers for the
full name tables):
- api_server delegators forward to sibling-module seams; the constant
  _api_server=sys.modules[__name__] kwarg is dropped from recorded args on
  BOTH sides. bytes (room grant secret) are modeled as JSON strings.
- discussion dataclasses are JSON objects with the dataclass fields;
  members always carry display_name (default "") and target (default null).
  Fixture rooms/events must carry the full field sets (the C args copies
  pass the scripted models through unnormalized).
- tuple-keyed watermark dicts project to [[thread, member], seq] arrays in
  insertion order on both sides.
- router.ROUTE_PENDING is scripted as {"__sentinel__": "ROUTE_PENDING"};
  the oracle returns the live singleton.
- async handlers are driven through asyncio.run; _read_json_body is an
  async recorder. Context-manager enter/exit are unobservable no-ops.
- integral JSON numbers model ints, fractional ones floats (a literal 5.0
  reads back as int 5 — the libjson identity exception).

Gate:
  bash tests/run_one_oracle.sh rest_revive4 oracle_rest_revive4.py "" "lib/libjson/json.o"
"""
import ast
import asyncio
import builtins
import copy
import __future__
import hashlib
import json
import math
import os
import re
import sys
import types
from dataclasses import asdict
from functools import partial
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(REPO / ".testbin" / "dbg_home_r4"))

FUT = __future__.annotations.compiler_flag
MISSING = object()


def norm(x):
    """Integral floats -> ints; tuples/sets -> lists (sets sorted); bytes ->
    str; Path -> posix; dataclasses are projected by the drivers first."""
    if isinstance(x, float):
        if math.isfinite(x) and x == int(x) and abs(x) < 1e16:
            return int(x)
        return x
    if isinstance(x, bytes):
        return x.decode("utf-8")
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    if isinstance(x, (set, frozenset)):
        return sorted(norm(v) for v in x)
    if isinstance(x, Path):
        return x.as_posix()
    return x


class NoopLock:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class CtxDouble:
    """Silent context manager yielding the scripted value (no trace event)."""
    def __init__(self, value):
        self.value = value

    def __enter__(self):
        return self.value

    def __exit__(self, *a):
        return False


class Raise:
    def __init__(self, spec):
        ty = spec.get("type", "Exception")
        msg = spec.get("message", "")
        self.spec = {"__raise__": ty, "message": msg}
        cls = EXC_REGISTRY.get(ty) or getattr(builtins, ty, None)
        if not (isinstance(cls, type) and issubclass(cls, BaseException)):
            cls = type(ty, (Exception,), {})
        self.exc = cls(msg)


class Script:
    def __init__(self, io_map):
        self.io = io_map or {}
        self.cursors = {}
        self.raised = []  # Raise instances handed out (identity round-trip)

    def _raise(self, spec):
        r = Raise(spec)
        self.raised.append(r)
        return r

    def next(self, name):
        entry = self.io.get(name, MISSING)
        if entry is MISSING:
            # RETURN a Raise (not raise) so make_rec records the event before
            # propagating — matches the C dispatcher's record-then-return.
            return self._raise({"type": "RuntimeError", "message": f"no script for {name}"})
        if isinstance(entry, list):
            i = self.cursors.get(name, 0)
            if i >= len(entry):
                return self._raise({"type": "RuntimeError",
                                    "message": f"script exhausted for {name}"})
            self.cursors[name] = i + 1
            entry = entry[i]
        if isinstance(entry, dict) and entry.get("__script__"):
            if "raise" in entry:
                return self._raise(entry["raise"])
            return entry.get("ret")
        return entry


# ── AST extraction ──────────────────────────────────────────────────────────

def compile_node(node, filename):
    mod = ast.Module(body=[node], type_ignores=[])
    ast.fix_missing_locations(mod)
    return compile(mod, filename, "exec", flags=FUT, dont_inherit=True)


def load_ns(relpath, seeds, want_funcs=(), want_classes=(), strict_consts=False):
    src = (QUARRY / relpath).read_text(encoding="utf-8")
    tree = ast.parse(src)
    modname = "quarry_r4_" + re.sub(r"\W", "_", relpath)
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update(seeds)
    ns["__name__"] = modname
    consts = [n for n in tree.body
              if isinstance(n, (ast.Assign, ast.AnnAssign))
              and n.value is not None
              and all(isinstance(t, ast.Name) for t in
                      (n.targets if isinstance(n, ast.Assign) else [n.target]))]

    def seed_consts(strict):
        for n in consts:
            try:
                exec(compile_node(n, relpath), ns)
            except Exception:
                if strict:
                    raise

    seed_consts(False)
    for n in tree.body:
        if isinstance(n, ast.ClassDef) and n.name in want_classes:
            exec(compile_node(n, relpath), ns)
    # re-seed constants that reference the classes (partial aliases)
    seed_consts(False)
    for n in ast.walk(tree):
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name in want_funcs:
            n2 = copy.copy(n)
            n2.decorator_list = []
            exec(compile_node(n2, relpath), ns)
    seed_consts(strict_consts)
    for name in list(want_funcs) + list(want_classes):
        assert name in ns, f"seed failure: {relpath}:{name}"
    return ns


def base_seeds(modname):
    import collections.abc as _abc
    logging_double = types.SimpleNamespace(
        getLogger=lambda name: types.SimpleNamespace(
            warning=lambda *a, **k: None, debug=lambda *a, **k: None,
            info=lambda *a, **k: None, error=lambda *a, **k: None))
    return {
        "__name__": modname,
        # Runtime isinstance sites (e.g. _terminal_text's `isinstance(result,
        # Mapping)`) need the REAL ABCs — seeding `object` makes every value
        # pass the guard and turns str inputs into AttributeError families.
        "Any": object, "Callable": _abc.Callable, "Optional": object,
        "Dict": dict, "Literal": object, "Mapping": _abc.Mapping,
        "Protocol": object, "Tuple": tuple, "Awaitable": _abc.Awaitable,
        "Iterable": _abc.Iterable, "Set": _abc.Set, "Sequence": _abc.Sequence,
        "dataclass": __import__("dataclasses").dataclass,
        "field": __import__("dataclasses").field,
        "contextlib": __import__("contextlib"),
        "functools": __import__("functools"),
        "partial": partial,
        "threading": types.SimpleNamespace(Lock=NoopLock),
        "logging": logging_double,
        "logger": logging_double.getLogger("quarry"),
        "time": types.SimpleNamespace(monotonic=lambda: 0.0, time=lambda: 0.0),
        "asyncio": types.SimpleNamespace(Event=object, Lock=NoopLock,
                                         CancelledError=type("CancelledError", (Exception,), {})),
        "math": math, "re": re, "json": json, "hashlib": hashlib,
        "os": types.SimpleNamespace(environ={}),
        "sys": sys,
        "uuid": types.SimpleNamespace(uuid4=lambda: types.SimpleNamespace(hex="u")),
    }


def pkg(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return sys.modules[name]


# Exception identity: sqlite3.Error kinds and the store failures must BE the
# classes the live except clauses catch.
SqliteError = type("Error", (Exception,), {})
SQLITE_KINDS = {}
for _k in ("DatabaseError", "IntegrityError", "InterfaceError", "InternalError",
           "NotSupportedError", "OperationalError", "ProgrammingError", "DataError"):
    SQLITE_KINDS[_k] = type(_k, (SqliteError,), {})
SQLITE_KINDS["Error"] = SqliteError
STORE_KINDS = {k: type(k, (Exception,), {}) for k in
               ("TransientStoreError", "PermanentStoreError", "StoreUnavailable", "StoreReplaced")}
EXC_REGISTRY = dict(SQLITE_KINDS)
EXC_REGISTRY.update(STORE_KINDS)

# ── module namespaces ───────────────────────────────────────────────────────

NS_COMMON = load_ns(
    "gateway/hosted_rooms_common.py", base_seeds("quarry_r4_common"),
    want_funcs=("identifier", "exact_fields", "bounded_int", "compact_json"))

_common_double = types.SimpleNamespace(
    identifier=NS_COMMON["identifier"], exact_fields=NS_COMMON["exact_fields"],
    bounded_int=NS_COMMON["bounded_int"], compact_json=NS_COMMON["compact_json"])

# driver constants (extracted straight from the AST; no import side effects)
_drv_src = (QUARRY / "gateway/hosted_room_driver.py").read_text(encoding="utf-8")
_drv_tree = ast.parse(_drv_src)
DRV_CONSTS = {}
for _n in _drv_tree.body:
    if isinstance(_n, ast.Assign):
        for _t in _n.targets:
            if isinstance(_t, ast.Name) and _t.id in (
                    "MAX_IDENTIFIER_CHARS", "MAX_PROMPT_BYTES",
                    "_TASK_PAYLOAD_REQUIRED_FIELDS", "_TASK_PAYLOAD_OPTIONAL_FIELDS"):
                _d = {}
                exec(ast.get_source_segment(_drv_src, _n), {"frozenset": frozenset}, _d)
                DRV_CONSTS[_t.id] = _d[_t.id]
assert set(DRV_CONSTS) == {"MAX_IDENTIFIER_CHARS", "MAX_PROMPT_BYTES",
                           "_TASK_PAYLOAD_REQUIRED_FIELDS", "_TASK_PAYLOAD_OPTIONAL_FIELDS"}

# HostedTaskIdentity + its identifier validator (real, from persistence)
_hti_seeds = base_seeds("quarry_r4_hti")
_hti_seeds["DriverStateError"] = type("DriverStateError", (Exception,), {})
NS_HTI = load_ns(
    "persistence/gateway/hosted_rooms.py", _hti_seeds,
    want_funcs=("_task_identifier",),
    want_classes=("DriverValidationError", "HostedTaskIdentity"))

# CONTROL_FRAME_OPENERS (module constant the discussion regex reads)
_pb_src = (QUARRY / "agent/prompt_builder.py").read_text(encoding="utf-8")
_pb_tree = ast.parse(_pb_src)
CF_OPENERS = None
for _n in _pb_tree.body:
    if isinstance(_n, ast.Assign) and any(
            isinstance(t, ast.Name) and t.id == "CONTROL_FRAME_OPENERS" for t in _n.targets):
        CF_OPENERS = ast.literal_eval(_n.value)
assert CF_OPENERS is not None

_hrd_seeds = base_seeds("quarry_r4_hrd")
_hrd_seeds.update({
    "common": _common_double,
    "compact_json": NS_COMMON["compact_json"],
    "driver": types.SimpleNamespace(**DRV_CONSTS),
    "router": types.SimpleNamespace(ROUTE_PENDING=object()),
    "lead_routing": types.SimpleNamespace(),
    "hosted_rooms": types.SimpleNamespace(),
    "scheduled_post_kind": lambda actor: None,
    "validate_context": lambda **kw: kw,
    "validate_task_input": lambda value: value,
    "HostedTaskIdentity": NS_HTI["HostedTaskIdentity"],
    "CONTROL_FRAME_OPENERS": CF_OPENERS,
})
NS_HRD = load_ns(
    "gateway/hosted_room_discussion.py", _hrd_seeds,
    want_funcs=(
        "reconstruct_task_plan", "plan_next_task", "plan_publication",
        "_validate_member_target", "_make_task_plan", "resolve_mentions",
        "_unaddressed_member_mentions", "_member_by_id", "derive_member_watermarks",
        "_derive_member_watermarks", "_effective_watermarks", "_rotate",
        "_pending_discussion", "_thread_messages", "_scheduled_job",
        "_first_round_responders", "_requires_direct_reply", "_event_attachments",
        "_terminal_text", "_truncate_utf8_text", "is_pass_text", "_member_actor", "_member_digest",
        "_peer_id", "_turn_coordinates", "_reconstructed_lead_routing",
        "_settled_effects", "_failed_effects", "_cancelled_effects", "_deferred_effects",
        "_positive_int", "_all_failure_reasons", "turn_round"),
    want_classes=(
        "DiscussionPolicyError", "DiscussionValidationError",
        "DiscussionReconstructionError", "DiscussionLimits", "DiscussionMember",
        "DiscussionRoom", "DiscussionTaskPlan", "DiscussionDecision",
        "EventPlan", "PublicationPlan", "_ValidatedEvent"))

_rg_seeds = base_seeds("quarry_r4_rg")
_rg_seeds["sqlite3"] = types.SimpleNamespace(**SQLITE_KINDS)
_rg_seeds["STORE_FAILURES"] = tuple(STORE_KINDS.values())
_rg_seeds["web"] = types.SimpleNamespace(json_response=lambda body, status=200: None)
NS_RG = load_ns(
    "gateway/platforms/api_server_room_grants.py", _rg_seeds,
    want_funcs=(
        "_handle_room_member_grant_refresh", "_handle_room_member_invitation",
        "_handle_room_member_grant_revoke", "_handle_room_member_capabilities",
        "_room_grant_token", "_room_grant_secret", "_room_grant_claims",
        "_decode_request_grant", "_room_store", "_json_error",
        "_require_unchanged_execution_policy", "_room_grant_error_response",
        "_hard_expiry", "_room_identity", "_local_target", "_issue_invitation"),
    want_classes=("RoomGrantReauthorizationRequired",))
EXC_REGISTRY["RoomGrantReauthorizationRequired"] = NS_RG["RoomGrantReauthorizationRequired"]

_asg_seeds = base_seeds("quarry_r4_asg")
NS_ASG = load_ns(
    "gateway/platforms/api_server.py", _asg_seeds,
    want_funcs=(
        "_declared_conversation_session", "_run_idempotency_scope",
        "_room_grant_token", "_room_grant_secret", "_room_grant_claims",
        "_normalize_room_dispatch", "_durable_run_status"))

# in-function import seams resolve through these packages
pkg("gateway")
pkg("tools")


def seam_import(modname, attr, import_seam, rec, call_seam, call_argb, make=None):
    mod = types.ModuleType(modname)

    def mod_getattr(name):
        if name != attr:
            raise AttributeError(name)
        rec(import_seam, lambda a, k: {})()
        return rec(call_seam, call_argb, make=make)

    mod.__getattr__ = mod_getattr
    sys.modules[modname] = mod
    parent, _, child = modname.rpartition(".")
    if parent:
        setattr(pkg(parent), child, mod)
    return mod


def multi_import(modname, import_seam, rec, names, argbs, makes=None, consts=()):
    """`from mod import a, b` — ONE import event on first attribute access.
    Names in `consts` are module CONSTANTS: their getattr records a value-fetch
    event and returns the scripted value itself (live binds a number, not a
    callable); other names return recorders for later calls."""
    mod = types.ModuleType(modname)
    fired = [False]
    makes = makes or {}

    def mod_getattr(name):
        if name not in names:
            raise AttributeError(name)
        if not fired[0]:
            rec(import_seam, lambda a, k: {})()
            fired[0] = True
        if name in consts:
            return rec(name, lambda a, k: {})()
        return rec(name, argbs[name], make=makes.get(name))

    mod.__getattr__ = mod_getattr
    sys.modules[modname] = mod
    parent, _, child = modname.rpartition(".")
    if parent:
        setattr(pkg(parent), child, mod)
    return mod


def make_rec(script, trace, armed):
    def rec(name, argb, make=None):
        def call(*a, **k):
            r = script.next(name)
            ev = {"ev": name, "args": argb(a, k), "ret": None}
            if isinstance(r, Raise):
                ev["ret"] = r.spec
                if armed[0]:
                    trace.append(ev)
                raise r.exc
            ev["ret"] = norm(r)
            if armed[0]:
                trace.append(ev)
            return make(r) if make else r
        return call
    return rec


def minus_io(work):
    work.pop("io", None)
    return work


def ns_obj(r):
    if isinstance(r, dict):
        return types.SimpleNamespace(**{k: ns_obj(v) for k, v in r.items()})
    if isinstance(r, list):
        return [ns_obj(v) for v in r]
    return r

# ── discussion model doubles ────────────────────────────────────────────────

def member_double(d):
    return NS_HRD["DiscussionMember"](
        member_id=d["member_id"], profile=d["profile"], handle=d["handle"],
        display_name=d.get("display_name", ""), target=d.get("target"))


def room_double(d):
    return NS_HRD["DiscussionRoom"](
        room_id=d["room_id"], name=d["name"],
        members=tuple(member_double(m) for m in d["members"]),
        gateway_id=d["gateway_id"], authority_epoch=d["authority_epoch"])


def event_double(d):
    return NS_HRD["_ValidatedEvent"](
        raw=d.get("raw", {}), seq=d["seq"], event_id=d["event_id"],
        kind=d["kind"], actor=d.get("actor", {}), payload=d.get("payload", {}))


def identity_double(d):
    return NS_HTI["HostedTaskIdentity"](
        room_id=d["room_id"], task_id=d["task_id"],
        thread_id=d["thread_id"], turn_id=d["turn_id"])


def task_double(d):
    return NS_HRD["DiscussionTaskPlan"](
        identity=identity_double(d["identity"]), payload=d["payload"],
        discussion_event_id=d["discussion_event_id"], member=member_double(d["member"]),
        member_index=d["member_index"], round_index=d["round_index"],
        seen_through_seq=d["seen_through_seq"])


def lead_round_double(d):
    if d is None:
        return None
    return types.SimpleNamespace(
        task=task_double(d["task"]) if d.get("task") else None,
        routing_task_id=d.get("routing_task_id"),
        responders=tuple(member_double(m) for m in d.get("responders", [])),
        lead=member_double(d["lead"]) if d.get("lead") else None)


def wm_from_pairs(pairs):
    if pairs is None:
        return None
    return {(t, m): v for (t, m), v in pairs}


def wm_proj(d):
    return [[list(k), v] for k, v in d.items()]


def events_arg(events):
    return [asdict(e) for e in events]


class StoreDict(dict):
    pass


def hrd_install_room_seams(rec):
    NS_HRD["validate_room"] = rec(
        "validate_room", lambda a, k: {"room_value": a[0], "local_profiles": k["local_profiles"]},
        make=room_double)
    NS_HRD["_validated_events"] = rec(
        "validated_events", lambda a, k: {"events": a[0], "room": asdict(k["room"])},
        make=lambda r: [event_double(e) for e in r])


def hrd_install_driver(rec):
    NS_HRD["driver"] = types.SimpleNamespace(
        **DRV_CONSTS,
        validate_bound_task_manifest=rec("validate_bound_task_manifest",
                                         lambda a, k: {"value": a[0]}))


def gateway_import_seam(rec, hr_double):
    g = pkg("gateway")
    g.__dict__.pop("hosted_rooms", None)

    def ga(name):
        if name == "hosted_rooms":
            rec("import_hosted_rooms", lambda a, k: {})()
            # Cache the attribute: a real module import binds it on the
            # package, so later `from gateway import hosted_rooms` statements
            # (and _handle_fromlist's hasattr probe) do NOT re-fire.
            g.hosted_rooms = hr_double
            return hr_double
        raise AttributeError(name)

    g.__getattr__ = ga


def persistence_seams(rec):
    multi_import(
        "persistence_assembly", "import_persistence_assembly", rec,
        ["open_persistence", "profile_scope"],
        {"profile_scope": lambda a, k: {},
         "open_persistence": lambda a, k: {"scope": a[0]}},
        makes={"open_persistence": CtxDouble})


def rg_self(rec, **extra):
    base = dict(
        _room_grant_token=rec("room_grant_token", lambda a, k: {"request": a[0]}),
        _room_grant_secret=rec("room_grant_secret", lambda a, k: {}))
    base.update(extra)
    return types.SimpleNamespace(**base)


def rg_web(rec):
    NS_RG["web"] = types.SimpleNamespace(
        json_response=rec("web_json_response",
                          lambda a, k: {"body": a[0], "status": k.get("status", 200)}))


def async_rec(rec, name, argb, make=None):
    sync = rec(name, argb, make=make)

    async def call(*a, **k):
        return sync(*a, **k)
    return call


def handler_kwargs(rec, work):
    return dict(
        _openai_error=rec("openai_error", lambda a, k: {"message": a[0], **k}),
        _api_request_profile=types.SimpleNamespace(get=lambda: None))


def run_async(coro):
    return asyncio.run(coro)


# ── api_server drivers ──────────────────────────────────────────────────────

def d_declared_conversation_session(work, trace, script):
    rec = make_rec(script, trace, [True])
    seam_import("gateway.session_api", "declared_api_session",
                "import_declared_api_session", rec, "declared_api_session",
                lambda a, k: {"db": a[0], "key": a[1],
                              "persistence": dict(k["persistence"])
                              if isinstance(k["persistence"], dict) else k["persistence"]})
    routing = types.SimpleNamespace(
        latest_session_for_peer=rec(
            "latest_session_for_peer",
            lambda a, k: {"source": k["source"], "session_key": k["session_key"]},
            make=ns_obj))

    def store_make(r):
        if isinstance(r, dict):
            s = StoreDict(r)
            s.routing = routing
            return s
        return r

    dbl = types.SimpleNamespace(
        _ensure_store=rec("ensure_store", lambda a, k: {}),
        _store_beside=rec("store_beside", lambda a, k: {"db": a[0]}, make=store_make),
        gateway_runner=types.SimpleNamespace(
            session_authority=object() if work.get("session_authority_present") else None),
        name="gw",
        _SESSION_SOURCE=work.get("session_source", ""))
    return NS_ASG["_declared_conversation_session"](dbl, work.get("gateway_session_key"))


def d_run_idempotency_scope(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_ASG["_api_runs"] = types.SimpleNamespace(
        _run_idempotency_scope=rec("api_runs.run_idempotency_scope",
                                   lambda a, k: {"self": a[0], "request": a[1]}))
    return NS_ASG["_run_idempotency_scope"](work.get("self"), work.get("request"))


def d_room_grant_token(work, trace, script):
    NS_ASG["_room_grants"] = types.SimpleNamespace(
        _room_grant_token=NS_RG["_room_grant_token"])
    req = work.get("request") or {}
    headers = req.get("headers") if isinstance(req, dict) else None
    return NS_ASG["_room_grant_token"](types.SimpleNamespace(headers=headers or {}))


def d_room_grant_secret(work, trace, script):
    rec = make_rec(script, trace, [True])
    seam_import("gateway.hosted_room_peer", "gateway_room_grant_secret",
                "import_gateway_room_grant_secret", rec, "gateway_room_grant_secret",
                lambda a, k: {},
                make=lambda r: r.encode("utf-8") if isinstance(r, str) else r)
    NS_ASG["_room_grants"] = types.SimpleNamespace(
        _room_grant_secret=NS_RG["_room_grant_secret"])
    return NS_ASG["_room_grant_secret"](work.get("self"))


def d_room_grant_claims(work, trace, script):
    rec = make_rec(script, trace, [True])
    seam_import("gateway.hosted_room_peer", "decode_room_grant",
                "import_decode_room_grant", rec, "decode_room_grant",
                lambda a, k: {"secret": a[0], "token": a[1], "permission": k["permission"]})
    hr = types.SimpleNamespace(
        room_grant_is_revoked=rec("room_grant_is_revoked",
                                  lambda a, k: {"store": a[0], "placement": k["placement"],
                                                "claims": k["claims"]}),
        peer_room_grant_is_current=rec("peer_room_grant_is_current",
                                       lambda a, k: {"store": a[0], "placement": k["placement"],
                                                     "claims": k["claims"]}))
    gateway_import_seam(rec, hr)
    persistence_seams(rec)
    NS_ASG["_room_grants"] = types.SimpleNamespace(
        _room_grant_claims=NS_RG["_room_grant_claims"])
    dbl = rg_self(rec)
    return NS_ASG["_room_grant_claims"](dbl, work.get("request"),
                                        permission=work.get("permission", "status"))


def d_normalize_room_dispatch(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_ASG["_room_dispatch"] = types.SimpleNamespace(
        _normalize_room_dispatch=async_rec(
            rec, "room_dispatch.normalize_room_dispatch",
            lambda a, k: {"self": a[0], "request": a[1], "body": a[2]}))
    return run_async(NS_ASG["_normalize_room_dispatch"](
        work.get("self"), work.get("request"), work.get("body")))


def d_durable_run_status(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_ASG["_api_runs"] = types.SimpleNamespace(
        _durable_run_status=rec("api_runs.durable_run_status",
                                lambda a, k: {"self": a[0], "request": a[1], "run_id": a[2]}))
    return NS_ASG["_durable_run_status"](work.get("self"), work.get("request"),
                                         work.get("run_id"))


def d_handle_room_member_capabilities(work, trace, script):
    rec = make_rec(script, trace, [True])
    rg_web(rec)
    NS_RG["_local_target"] = rec("local_target", lambda a, k: {"claims": a[0]})
    NS_RG["_local_room_catalog"] = rec(
        "local_room_catalog", lambda a, k: {"profile": a[1], "installation_id": a[2]})
    dbl = rg_self(rec, _room_grant_claims=rec(
        "room_grant_claims", lambda a, k: {"request": a[0], "permission": k["permission"]}))
    return run_async(NS_RG["_handle_room_member_capabilities"](
        dbl, work.get("request"), **handler_kwargs(rec, work)))


def d_handle_room_member_grant_revoke(work, trace, script):
    rec = make_rec(script, trace, [True])
    rg_web(rec)
    seam_import("gateway.hosted_room_peer", "decode_room_grant",
                "import_decode_room_grant", rec, "decode_room_grant",
                lambda a, k: {"secret": a[0], "token": a[1], "permission": k["permission"]})
    NS_RG["_local_target"] = rec("local_target", lambda a, k: {"claims": a[0]})
    hr = types.SimpleNamespace(
        revoke_room_grant_scope=rec(
            "revoke_room_grant_scope",
            lambda a, k: {"store": a[0], "placement": k["placement"],
                          "claims": k["claims"], "expires_at": k["expires_at"]}))
    gateway_import_seam(rec, hr)
    persistence_seams(rec)
    dbl = rg_self(rec, _read_json_body=async_rec(
        rec, "read_json_body", lambda a, k: {"request": a[0]}))
    return run_async(NS_RG["_handle_room_member_grant_revoke"](
        dbl, work.get("request"), **handler_kwargs(rec, work)))


def d_handle_room_member_grant_refresh(work, trace, script):
    rec = make_rec(script, trace, [True])
    rg_web(rec)
    multi_import("gateway.hosted_room_peer", "import_hosted_room_peer", rec,
                 ["MAX_DISPATCH_GRANT_TTL_SECONDS", "issue_room_grant"],
                 {"MAX_DISPATCH_GRANT_TTL_SECONDS": lambda a, k: {},
                  "issue_room_grant": lambda a, k: {"secret": a[0], **k}},
                 consts=("MAX_DISPATCH_GRANT_TTL_SECONDS",))
    multi_import("gateway.hosted_room_execution_policy",
                 "import_hosted_room_execution_policy", rec,
                 ["execution_policy_mapping"],
                 {"execution_policy_mapping": lambda a, k: {"target_profile": k["target_profile"]}})
    NS_RG["_local_target"] = rec("local_target", lambda a, k: {"claims": a[0]})
    NS_RG["time"] = types.SimpleNamespace(time=rec("time", lambda a, k: {}))
    NS_RG["uuid"] = types.SimpleNamespace(
        uuid4=lambda: types.SimpleNamespace(hex=rec("uuid4_hex", lambda a, k: {})()))
    dbl = rg_self(rec,
                  _read_json_body=async_rec(rec, "read_json_body",
                                            lambda a, k: {"request": a[0]}),
                  _room_grant_claims=rec("room_grant_claims",
                                         lambda a, k: {"request": a[0],
                                                       "permission": k["permission"]}),
                  _profile_scope=rec("profile_scope_ctx",
                                     lambda a, k: {"profile": a[0]}, make=CtxDouble))
    return run_async(NS_RG["_handle_room_member_grant_refresh"](
        dbl, work.get("request"), **handler_kwargs(rec, work)))


def d_handle_room_member_invitation(work, trace, script):
    rec = make_rec(script, trace, [True])
    rg_web(rec)
    NS_RG["_local_target"] = rec("local_target", lambda a, k: {"claims": a[0]})
    NS_RG["_issue_invitation"] = rec(
        "issue_invitation",
        lambda a, k: {"self": work.get("self"), "body": a[1], "profile": a[2]})
    dbl = rg_self(rec,
                  _check_auth=rec("check_auth", lambda a, k: {"request": a[0]}),
                  _read_json_body=async_rec(rec, "read_json_body",
                                            lambda a, k: {"request": a[0]}))
    return run_async(NS_RG["_handle_room_member_invitation"](
        dbl, work.get("request"), **handler_kwargs(rec, work)))

# ── discussion drivers ──────────────────────────────────────────────────────

def d_validate_member_target(work, trace, script):
    return NS_HRD["_validate_member_target"](
        work.get("value"), profile=work.get("profile", ""),
        known_profiles=set(work.get("known_profiles", [])),
        index=work.get("index", 0))


def d_resolve_mentions(work, trace, script):
    members = [member_double(m) for m in work.get("members", [])]
    out = NS_HRD["resolve_mentions"](
        work.get("texts", []), members, default_all=work.get("default_all", True))
    return [asdict(m) for m in out]


def d_unaddressed_member_mentions(work, trace, script):
    messages = [event_double(m) for m in work.get("messages", [])]
    room = room_double(work["room"])
    out = NS_HRD["_unaddressed_member_mentions"](messages, room)
    return [asdict(m) for m in out]


def d_member_by_id(work, trace, script):
    room = room_double(work["room"])
    return asdict(NS_HRD["_member_by_id"](room, work.get("member_id")))


def d_derive_member_watermarks(work, trace, script):
    rec = make_rec(script, trace, [True])
    hrd_install_room_seams(rec)
    out = NS_HRD["derive_member_watermarks"](
        work.get("room_value"), work.get("events", []),
        local_profiles=work.get("local_profiles", []))
    return wm_proj(out)


def d_make_task_plan(work, trace, script):
    rec = make_rec(script, trace, [True])
    hrd_install_driver(rec)
    out = NS_HRD["_make_task_plan"](
        room=room_double(work["room"]),
        discussion_event=event_double(work["discussion_event"]),
        member=member_double(work["member"]),
        member_index=work.get("member_index", 0),
        round_index=work.get("round_index", 0),
        seen_through_seq=work.get("seen_through_seq", 0),
        prompt=work.get("prompt", ""),
        input_context=work.get("input_context"),
        attachments=work.get("attachments", ()),
        lead_routing=work.get("lead_routing", False))
    return asdict(out)


def d_reconstruct_task_plan(work, trace, script):
    rec = make_rec(script, trace, [True])
    hrd_install_room_seams(rec)
    hrd_install_driver(rec)
    NS_HRD["validate_task_input"] = rec("validate_task_input", lambda a, k: {"value": a[0]})
    task = work.get("task") or {}
    ident = task.get("identity")
    task_arg = {
        "identity": identity_double(ident) if isinstance(ident, dict) else ident,
        "payload": task.get("payload"),
    }
    out = NS_HRD["reconstruct_task_plan"](
        work.get("room_value"), work.get("events", []), task_arg,
        local_profiles=work.get("local_profiles", []))
    return asdict(out)


def d_plan_publication(work, trace, script):
    rec = make_rec(script, trace, [True])
    hrd_install_room_seams(rec)
    NS_HRD["lead_routing"] = types.SimpleNamespace(
        is_routing_task=rec("lead_routing.is_routing_task",
                            lambda a, k: {"task": asdict(a[0])}),
        assignment_effect=rec("lead_routing.assignment_effect",
                              lambda a, k: {"result": a[0], "task": asdict(k["task"]),
                                            "room": asdict(k["room"]), "status": k["status"]}))
    NS_HRD["_all_failure_reasons"] = rec("all_failure_reasons", lambda a, k: {},
                                         make=frozenset)
    seam_import("tools.bot_failure_reasons", "classify_agent_error",
                "import_classify_agent_error", rec, "classify_agent_error",
                lambda a, k: {"error_text": a[0]})
    out = NS_HRD["plan_publication"](
        work.get("room_value"), work.get("events", []), task_double(work["task"]),
        status=work.get("status"), result=work.get("result"),
        execution_generation=work.get("execution_generation"),
        local_profiles=work.get("local_profiles", []))
    return asdict(out)


def d_plan_next_task(work, trace, script):
    rec = make_rec(script, trace, [True])
    hrd_install_room_seams(rec)
    hrd_install_driver(rec)
    NS_HRD["validate_context"] = rec("validate_context", lambda a, k: dict(k))
    NS_HRD["validate_task_input"] = rec("validate_task_input", lambda a, k: {"value": a[0]})
    NS_HRD["scheduled_post_kind"] = rec("scheduled_post_kind", lambda a, k: {"actor": a[0]})
    route_pending = object()

    def routed_make(r):
        if isinstance(r, dict) and r.get("__sentinel__") == "ROUTE_PENDING":
            return route_pending
        if isinstance(r, list):
            return tuple(member_double(m) for m in r)
        return r

    NS_HRD["router"] = types.SimpleNamespace(
        ROUTE_PENDING=route_pending,
        continue_count=rec("router.continue_count",
                           lambda a, k: {"validated": events_arg(a[0]),
                                         "discussion_event_id": a[1]}),
        continued_through=rec("router.continued_through",
                              lambda a, k: {"validated": events_arg(a[0])}),
        routed_responders=rec("router.routed_responders",
                              lambda a, k: {"room": asdict(a[0]), "discussion": asdict(a[1]),
                                            "validated": events_arg(a[2]), "context": dict(a[3])},
                              make=routed_make),
        resume_round=rec("router.resume_round",
                         lambda a, k: {"validated": events_arg(a[0]),
                                       "discussion_event_id": a[1],
                                       "member_messages": events_arg(a[2])}))
    NS_HRD["_lead_round"] = rec(
        "lead_round",
        lambda a, k: {"room": asdict(a[0]), "discussion": asdict(a[1]),
                      "validated": events_arg(a[2]), "context": dict(a[3])},
        make=lead_round_double)
    NS_HRD["_bounded_task_delta"] = rec(
        "bounded_task_delta",
        lambda a, k: {"thread_messages": events_arg(a[0]), "watermark": k["watermark"],
                      "member": asdict(k["member"])},
        make=lambda r: ([event_double(e) for e in r[0]], r[1]))
    NS_HRD["_build_prompt"] = rec(
        "build_prompt",
        lambda a, k: {"room": asdict(k["room"]), "member": asdict(k["member"]),
                      "messages": events_arg(k["messages"]), "watermark": k["watermark"],
                      "seen_through_seq": k["seen_through_seq"],
                      "context": dict(k["context"]) if k["context"] is not None else None,
                      "addressed_by_job": k["addressed_by_job"],
                      "reply_required": k["reply_required"],
                      "assigned_by": asdict(k["assigned_by"]) if k["assigned_by"] is not None else None,
                      "routed": k["routed"]})
    limits = work.get("limits")
    limits_obj = NS_HRD["DiscussionLimits"](**limits) if limits else NS_HRD["DiscussionLimits"]()
    out = NS_HRD["plan_next_task"](
        work.get("room_value"), work.get("events", []),
        local_profiles=work.get("local_profiles", []),
        initial_watermarks=wm_from_pairs(work.get("initial_watermarks")),
        freeze_input_context=work.get("freeze_input_context", False),
        limits=limits_obj,
        room_context=work.get("room_context"))
    return asdict(out)


DRIVERS = {
    "declared_conversation_session": d_declared_conversation_session,
    "run_idempotency_scope": d_run_idempotency_scope,
    "room_grant_token": d_room_grant_token,
    "room_grant_secret": d_room_grant_secret,
    "room_grant_claims": d_room_grant_claims,
    "normalize_room_dispatch": d_normalize_room_dispatch,
    "durable_run_status": d_durable_run_status,
    "handle_room_member_capabilities": d_handle_room_member_capabilities,
    "handle_room_member_grant_revoke": d_handle_room_member_grant_revoke,
    "handle_room_member_grant_refresh": d_handle_room_member_grant_refresh,
    "handle_room_member_invitation": d_handle_room_member_invitation,
    "validate_member_target": d_validate_member_target,
    "resolve_mentions": d_resolve_mentions,
    "unaddressed_member_mentions": d_unaddressed_member_mentions,
    "member_by_id": d_member_by_id,
    "derive_member_watermarks": d_derive_member_watermarks,
    "make_task_plan": d_make_task_plan,
    "plan_next_task": d_plan_next_task,
    "reconstruct_task_plan": d_reconstruct_task_plan,
    "plan_publication": d_plan_publication,
}


def run_case(driver, inp):
    trace = []
    work = copy.deepcopy(inp)
    script = Script(work.get("io"))
    try:
        ret = driver(work, trace, script)
        minus_io(work)
        return {"ret": norm(ret), "trace": norm(trace), "state": norm(work)}
    except BaseException as e:  # SystemExit included — fail closed per case
        # Scripted raises round-trip BARE: str(KeyError(k)) is repr(k), and
        # multi-arg constructors mangle under cls(msg) — identity-match the
        # exact instance the script handed out and reuse its spec unchanged.
        spec = None
        for r in script.raised:
            if e is r.exc:
                spec = r.spec
                break
        if spec is None:
            spec = {"__raise__": type(e).__name__, "message": str(e)}
        minus_io(work)
        return {"ret": spec, "trace": norm(trace), "state": norm(work)}


def main():
    n = 0
    mismatches = 0
    shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec = json.loads(line)
        except Exception as e:
            mismatches += 1
            print(f"BAD JSON from harness: {e}")
            continue
        n += 1
        func = rec.get("func", "")
        inp = rec.get("in") or {}
        got = rec.get("out")
        driver = DRIVERS.get(func)
        if driver is None:
            mismatches += 1
            print(f"UNKNOWN func {func!r}")
            continue
        if not isinstance(got, dict) or "error" in got:
            mismatches += 1
            print(f"HARNESS ERROR {func}: {got}")
            continue
        try:
            want = run_case(driver, inp)
        except BaseException as e:
            mismatches += 1
            print(f"ORACLE DRIVER FAILURE {func}: {type(e).__name__}: {e}")
            continue
        if got != want:
            mismatches += 1
            if shown < 8:
                shown += 1
                print(f"MISMATCH {func} case#{n}")
                for key in ("ret", "trace", "state"):
                    if got.get(key) != want.get(key):
                        print(f"  {key} C  : {json.dumps(got.get(key), ensure_ascii=False)[:600]}")
                        print(f"  {key} PY : {json.dumps(want.get(key), ensure_ascii=False)[:600]}")
    print(f"ORACLE rest_revive4: {n} cases, {mismatches} mismatches")
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()
