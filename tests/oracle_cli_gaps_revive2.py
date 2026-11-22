#!/usr/bin/env python3
"""oracle_cli_gaps_revive2.py — differential oracle for the cli_gaps group-2 revival.

Reads the harness JSONL on stdin ({"func","in","out"}), AST-extracts the LIVE
quarry defs, seeds module constants (Assign + AnnAssign), drives each live
function under the SAME scripted seam results the C harness consumed, and
compares ordered trace + ret + post-state. BaseException is caught per case
(SystemExit included); missing scripts fail closed on both sides.

Gate:
  bash tests/run_one_oracle.sh cli_gaps_revive2 oracle_cli_gaps_revive2.py "" "src/cli/cli_gaps.o lib/libjson/json.o"
"""
import sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
import ast
import builtins
import copy
import __future__
import hashlib
import hmac
import json
import logging as _logging_real  # noqa: F401  (kept real for reference)
import math
import os
import re
import sys
import types
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
os.environ.setdefault("HOME", str(REPO.parent))
os.environ.setdefault("USERPROFILE", str(REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(REPO / ".testbin" / "dbg_home_cg2"))

FUT = __future__.annotations.compiler_flag
MISSING = object()


def norm(x):
    """Integral floats -> ints (libjson emits %.0f); tuples -> lists; Path -> posix."""
    if isinstance(x, float):
        if math.isfinite(x) and x == int(x) and abs(x) < 1e16:
            return int(x)
        return x
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    if isinstance(x, Path):
        return x.as_posix()
    return x


class NoopLock:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class Raise:
    def __init__(self, spec):
        ty = spec.get("type", "Exception")
        msg = spec.get("message", "")
        self.spec = {"__raise__": ty, "message": msg}
        cls = getattr(builtins, ty, None)
        if not (isinstance(cls, type) and issubclass(cls, BaseException)):
            cls = type(ty, (Exception,), {})
        self.exc = cls(msg)


class Script:
    def __init__(self, io_map):
        self.io = io_map or {}
        self.cursors = {}

    def next(self, name):
        entry = self.io.get(name, MISSING)
        if entry is MISSING:
            raise RuntimeError(f"no script for {name}")
        if isinstance(entry, list):
            i = self.cursors.get(name, 0)
            if i >= len(entry):
                raise RuntimeError(f"script exhausted for {name}")
            self.cursors[name] = i + 1
            entry = entry[i]
        if isinstance(entry, dict) and entry.get("__script__"):
            if "raise" in entry:
                return Raise(entry["raise"])
            return entry.get("ret")
        return entry


def untag(t):
    """Tagged float-convertible value -> Python value (null/num/str/bool)."""
    if t is None:
        return None
    k = t.get("kind", "null")
    if k == "null":
        return None
    return t.get("value")


# ── AST extraction ──────────────────────────────────────────────────────────

def compile_node(node, filename):
    mod = ast.Module(body=[node], type_ignores=[])
    ast.fix_missing_locations(mod)
    return compile(mod, filename, "exec", flags=FUT, dont_inherit=True)


def load_ns(relpath, seeds, want_funcs=(), want_classes=(), strict_consts=True):
    src = (QUARRY / relpath).read_text(encoding="utf-8")
    tree = ast.parse(src)
    modname = "quarry_cg2_" + re.sub(r"\W", "_", relpath)
    mod = types.ModuleType(modname)
    sys.modules[modname] = mod
    ns = mod.__dict__
    ns.update(seeds)
    # seeds may carry a shorter __name__ alias; py3.14 dataclasses resolves
    # annotations via sys.modules[cls.__module__] — register the alias too.
    alias = ns.get("__name__", modname)
    if alias != modname:
        sys.modules[alias] = mod
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
            # The C harness tags each lease with cg_id; LIVE slots forbid it.
            n.body = [b for b in n.body
                      if not (isinstance(b, ast.Assign)
                              and any(getattr(t, "id", "") == "__slots__"
                                      for t in b.targets))]
            exec(compile_node(n, relpath), ns)
        if isinstance(n, ast.FunctionDef) and n.name in want_funcs:
            exec(compile_node(n, relpath), ns)
    seed_consts(strict_consts)
    for name in list(want_funcs) + list(want_classes):
        assert name in ns, f"seed failure: {relpath}:{name}"
    return ns


def base_seeds(modname):
    logging_double = types.SimpleNamespace(
        getLogger=lambda name: types.SimpleNamespace(
            warning=lambda *a, **k: None, debug=lambda *a, **k: None,
            info=lambda *a, **k: None, error=lambda *a, **k: None))
    return {
        "__name__": modname,
        "Any": object, "Callable": object, "Optional": object, "Dict": object,
        "Literal": object, "Mapping": object, "Protocol": object, "Tuple": object,
        "Awaitable": object,
        "dataclass": __import__("dataclasses").dataclass,
        "field": __import__("dataclasses").field,
        "contextlib": __import__("contextlib"),
        "contextmanager": __import__("contextlib").contextmanager,
        "asynccontextmanager": __import__("contextlib").asynccontextmanager,
        "threading": types.SimpleNamespace(Lock=NoopLock),
        "weakref": types.SimpleNamespace(WeakKeyDictionary=dict),
        "logging": logging_double,
        "time": types.SimpleNamespace(monotonic=lambda: 0.0, time=lambda: 0.0),
        "asyncio": types.SimpleNamespace(Event=object, Lock=NoopLock,
                                         CancelledError=type("CancelledError", (Exception,), {})),
        "math": math, "re": re, "json": json, "hashlib": hashlib, "hmac": hmac,
        "os": types.SimpleNamespace(environ={}),
        "socket": types.SimpleNamespace(AF_UNIX=1),
        "secrets": types.SimpleNamespace(token_urlsafe=lambda n: "tok"),
        "sys": sys,
        "ContextVar": type("ContextVar", (), {
            "__init__": lambda self, name, default=None: setattr(self, "default", default),
            "get": lambda self: self.default,
        }),
    }


def pkg(name):
    if name not in sys.modules:
        m = types.ModuleType(name)
        m.__path__ = []
        sys.modules[name] = m
    return sys.modules[name]


# ── module namespaces (built once) ──────────────────────────────────────────

NS_DBREC = load_ns(
    "gateway/session_db_recovery.py", base_seeds("quarry_cg2_dbrec"),
    want_funcs=("_publish_health", "_registry_owned"),
    want_classes=("_Unavailable", "_HealthSource", "RecoverableHandleCache"))

NS_MWL = load_ns(
    "gateway/session_managed_worker_log.py", base_seeds("quarry_cg2_mwl"),
    want_funcs=("_lock_for",))

NS_SPL = load_ns(
    "gateway/session_producer_lifecycle.py", base_seeds("quarry_cg2_spl"),
    want_funcs=("_turns",))

NS_SRA = load_ns(
    "gateway/session_retry_admission.py", base_seeds("quarry_cg2_sra"),
    want_funcs=("take",))

NS_TS = load_ns(
    "agent/think_scrubber.py", base_seeds("quarry_cg2_ts"),
    want_classes=("StreamingThinkScrubber",))

_sct_seeds = base_seeds("quarry_cg2_sct")
_sct_seeds.update(THINK_OPEN_TAGS=NS_TS["THINK_OPEN_TAGS"],
                  THINK_CLOSE_TAGS=NS_TS["THINK_CLOSE_TAGS"],
                  _Scrubber=NS_TS["StreamingThinkScrubber"])
NS_SCT = load_ns(
    "gateway/stream_consumer_think.py", _sct_seeds,
    want_classes=("StreamThinkFilterMixin",))

NS_SD = load_ns(
    "gateway/systemd_notify.py", base_seeds("quarry_cg2_sd"),
    want_funcs=("_notify_socket", "watchdog_interval_seconds"),
    want_classes=("SystemdWatchdog",))

NS_TL = load_ns(
    "gateway/turn_lease.py", base_seeds("quarry_cg2_tl"),
    want_funcs=("_holder_desc",),
    want_classes=("TurnLeaseTimeoutError", "TurnLeaseToken", "_SessionLease",
                  "SessionTurnLeaseRegistry"))

NS_ZAI = load_ns(
    "hermes_cli/auth_zai_kimi.py", base_seeds("quarry_cg2_zai"),
    want_funcs=("_probe_single_zai_endpoint",))

NS_RET = load_ns(
    "hermes_cli/backend_retirement.py", base_seeds("quarry_cg2_ret"),
    want_classes=("RetirementFence",))

_ac_seeds = base_seeds("quarry_cg2_ac")
NS_CP = load_ns(
    "agent/credential_pool.py", _ac_seeds,
    want_funcs=("_normalize_custom_pool_name",), strict_consts=False)
_ac_seeds["CUSTOM_POOL_PREFIX"] = NS_CP["CUSTOM_POOL_PREFIX"]
_ac_seeds["_normalize_custom_pool_name"] = NS_CP["_normalize_custom_pool_name"]
NS_AC = load_ns(
    "hermes_cli/auth_commands.py", _ac_seeds,
    want_funcs=("_get_custom_provider_entries",), strict_consts=False)

NS_BC = load_ns(
    "hermes_cli/browser_credential.py", base_seeds("quarry_cg2_bc"),
    want_funcs=("current_if", "generation_of", "is_current", "rotate"))

NS_BUN = load_ns(
    "hermes_cli/bundles.py", base_seeds("quarry_cg2_bun"),
    want_funcs=("_fail",))

NS_CKP = load_ns(
    "hermes_cli/checkpoints.py", base_seeds("quarry_cg2_ckp"),
    want_funcs=("_confirm",))

NS_DOCK = load_ns(
    "hermes_cli/cli_process_dock.py", base_seeds("quarry_cg2_dock"),
    want_funcs=("_last_output_line", "process_status", "process_rows",
                "process_activity", "process_tail"))


# ── recording seam factory ──────────────────────────────────────────────────

def make_rec(script, trace, armed, trace_exhaust=False):
    def rec(name, argb, make=None):
        def call(*a, **k):
            ev = {"ev": name, "args": argb(a, k), "ret": None}
            if trace_exhaust:
                try:
                    r = script.next(name)
                except RuntimeError as exc:
                    ev["ret"] = {"__raise__": "RuntimeError", "message": str(exc)}
                    if armed[0]:
                        trace.append(ev)
                    raise
            else:
                r = script.next(name)
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


# ── per-function drivers ────────────────────────────────────────────────────

def d_is_stale(work, trace, script):
    cache = NS_DBREC["RecoverableHandleCache"](handles={}, lock=NoopLock(), clock=lambda: 0.0)
    cache._generation = work["self_generation"]
    u = NS_DBREC["_Unavailable"](error="e")
    slot = work.get("slot", "absent")
    if slot == "same":
        cache._unavailable = {"p": u}
    elif slot == "other":
        cache._unavailable = {"p": NS_DBREC["_Unavailable"]()}
    else:
        cache._unavailable = {}
    return cache._is_stale("p", u, work["generation"])


def d_last_error(work, trace, script):
    rec = make_rec(script, trace, [True])

    class FP:
        def __init__(self, raw):
            self.raw = raw

        def resolve(self):
            return rec("resolve", lambda a, k: {"path": self.raw})()

    NS_DBREC["Path"] = FP
    cache = NS_DBREC["RecoverableHandleCache"](handles={}, lock=NoopLock(), clock=lambda: 0.0)
    cache._unavailable = {FP(e["path"]): NS_DBREC["_Unavailable"](error=e["error"])
                          for e in work.get("unavailable", [])}
    return cache.last_error(work["path"])


def d_publish_health(work, trace, script):
    rec = make_rec(script, trace, [True])
    sources = {}
    states = {}
    for g in work.get("states", []):
        src = sources.setdefault(g["source"], NS_DBREC["_HealthSource"]())
        states[src] = {e["path"]: e["state"] for e in g["entries"]}
    NS_DBREC["_health_states"] = states
    src = sources.setdefault(work["source"], NS_DBREC["_HealthSource"]())
    mod = types.ModuleType("gateway.status")
    mod.publish_runtime_status = rec("publish_runtime_status",
                                     lambda a, k: {"session_store": k.get("session_store", a[0] if a else None)})
    sys.modules["gateway.status"] = mod
    pkg("gateway").status = mod
    try:
        NS_DBREC["_publish_health"](src, work["path"], work["state"])
        return None
    finally:
        rev = {obj: sid for sid, obj in sources.items()}
        work["states"] = [{"source": rev[obj],
                           "entries": [{"path": p, "state": s} for p, s in d.items()]}
                          for obj, d in states.items()]


def d_lock_for(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_MWL["threading"] = types.SimpleNamespace(Lock=rec("Lock", lambda a, k: {}))
    NS_MWL["_locks"] = dict(work.get("locks", {}))
    try:
        return NS_MWL["_lock_for"](work["path"])
    finally:
        work["locks"] = dict(NS_MWL["_locks"])


def d_turns(work, trace, script):
    authority = types.SimpleNamespace()
    if "producer_turns" in work:
        authority.producer_turns = copy.deepcopy(work["producer_turns"])
    try:
        ret = NS_SPL["_turns"](authority)
        return ret
    finally:
        work["producer_turns"] = copy.deepcopy(authority.producer_turns)


def d_take(work, trace, script):
    objects = {}

    def obj(i):
        if i is None:
            return None
        return objects.setdefault(i, object())

    bound = work.get("bound")
    rw = None
    if bound is not None:
        rw = types.SimpleNamespace(taken=bound["taken"], source=obj(bound["source"]),
                                   store=obj(bound["store"]))
    rec = make_rec(script, trace, [True])
    NS_SRA["_bound"] = types.SimpleNamespace(
        get=rec("_bound.get", lambda a, k: {}, make=lambda r: rw))
    event = types.SimpleNamespace()
    if "event_source" in work:
        event.source = obj(work["event_source"])
    try:
        ret = NS_SRA["take"](event, obj(work.get("store")))
        if ret is None:
            return None
        return {"taken": ret.taken, "source": bound["source"], "store": bound["store"]}
    finally:
        if rw is not None and bound is not None:
            bound["taken"] = rw.taken


def _think_obj(work, trace, script):
    obj = NS_SCT["StreamThinkFilterMixin"]()
    th = work.get("think") or {"buffer": "", "in_block": False, "accumulated": ""}
    obj._think_buffer = th["buffer"]
    obj._in_think_block = th["in_block"]
    obj._accumulated = th["accumulated"]

    def append_acc(text):
        r = script.next("append_accumulated")
        ev = {"ev": "append_accumulated", "args": {"text": text}, "ret": None}
        obj._accumulated += text  # state effect, identical on the C side
        if isinstance(r, Raise):
            ev["ret"] = r.spec
            trace.append(ev)
            raise r.exc
        ev["ret"] = norm(r)
        trace.append(ev)
        return r

    obj._append_accumulated = append_acc
    return obj, th


def _think_writeback(work, obj, th):
    th["buffer"] = obj._think_buffer
    th["in_block"] = obj._in_think_block
    th["accumulated"] = obj._accumulated
    work["think"] = th


def d_filter(work, trace, script):
    obj, th = _think_obj(work, trace, script)
    try:
        obj._filter_and_accumulate(work["text"])
        return None
    finally:
        _think_writeback(work, obj, th)


def d_flush(work, trace, script):
    obj, th = _think_obj(work, trace, script)
    try:
        obj._flush_think_buffer()
        return None
    finally:
        _think_writeback(work, obj, th)


def d_watchdog(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_SD["os"] = types.SimpleNamespace(
        environ=types.SimpleNamespace(get=rec("environ.get",
                                              lambda a, k: {"key": a[0], "default": a[1] if len(a) > 1 else ""})))
    NS_SD["socket"] = (types.SimpleNamespace(AF_UNIX=1) if work.get("af_unix", True)
                       else types.SimpleNamespace())
    return NS_SD["watchdog_interval_seconds"]()


def d_lag_tol(work, trace, script):
    s = work["self"]
    dbl = types.SimpleNamespace(interval_seconds=s["interval_seconds"],
                                _lag_tolerance_seconds=untag(s.get("lag_tolerance_seconds")))
    fn = types.MethodType(NS_SD["SystemdWatchdog"]._lag_tolerance, dbl)
    return fn()


def d_record_tick(work, trace, script):
    s = work["self"]
    NS_SD["os"] = types.SimpleNamespace(environ={})
    NS_SD["socket"] = types.SimpleNamespace(AF_UNIX=1)
    w = NS_SD["SystemdWatchdog"](config_enabled=s["config_enabled"],
                                 lag_tolerance_seconds=untag(s.get("lag_tolerance_seconds")))
    w.interval_seconds = s["interval_seconds"]
    w._stopping = s["stopping"]
    w._unhealthy = s["unhealthy"]
    rec = make_rec(script, trace, [True])
    NS_SD["notify"] = rec("notify", lambda a, k: {"message": a[0]})
    try:
        return w.record_tick(scheduled_at=untag(work["scheduled_at"]), now=untag(work["now"]))
    finally:
        s["unhealthy"] = w._unhealthy


class LockDouble:
    def __init__(self, locked, script=None, trace=None, armed=None):
        self._locked = locked
        self.script = script
        self.trace = trace
        self.armed = armed

    def locked(self):
        return self._locked

    def release(self):
        if self.script is not None:
            r = self.script.next("lock.release")
            ev = {"ev": "lock.release", "args": {}, "ret": norm(r) if not isinstance(r, Raise) else r.spec}
            if self.armed[0]:
                self.trace.append(ev)
            if isinstance(r, Raise):
                raise r.exc
        self._locked = False


def _tl_setup(work, trace, script):
    armed = [False]
    rec = make_rec(script, trace, armed)
    NS_TL["time"] = types.SimpleNamespace(time=rec("time.time", lambda a, k: {}))
    NS_TL["asyncio"] = types.SimpleNamespace(Lock=lambda: LockDouble(False))
    reg = NS_TL["SessionTurnLeaseRegistry"](max_entries=work.get("max_entries", 512))
    leases = {}
    lease_by_id = {}
    for l in work.get("leases", []):
        lease = NS_TL["_SessionLease"]()
        lease.cg_id = l["id"]
        lease.lock = LockDouble(l["locked"])
        lease.pending_acquires = l["pending"]
        lease.last_used = l["last_used"]
        lease.acquired_at = l.get("acquired_at", 0.0)
        leases[l["sid"]] = lease
        lease_by_id[l["id"]] = lease
    tokens = {}
    tok = work.get("token")
    token = None
    if tok is not None:
        token = NS_TL["TurnLeaseToken"](tok["session_id"], tok["owner_key"],
                                        tok["generation"], lease=lease_by_id[tok["lease"]])
        token.released = tok["released"]
        tokens[tok["id"]] = token
    for ot in work.get("other_tokens", []):
        t = NS_TL["TurnLeaseToken"](ot.get("session_id", ""), ot["owner_key"],
                                    ot["generation"], lease=lease_by_id.get(ot.get("lease"), None))
        tokens[ot["id"]] = t
    for l in work.get("leases", []):
        lease = lease_by_id[l["id"]]
        h = l.get("holder")
        if h:
            if h not in tokens:
                tokens[h] = NS_TL["TurnLeaseToken"]("", "?", 0, lease=None)
            lease.holder = tokens[h]
        else:
            lease.holder = None
    reg._leases = leases
    NS_TL["logger"] = types.SimpleNamespace(
        warning=rec("logger.warning", lambda a, k: {"fmt": a[0], "args": list(a[1:])}),
        debug=rec("logger.debug", lambda a, k: {"fmt": a[0], "args": list(a[1:])}))
    armed[0] = True
    return reg, token, tokens, lease_by_id, rec, armed


def _tl_writeback(work, reg, token, tokens):
    rev = {obj: tid for tid, obj in tokens.items()}
    out = []
    for sid, lease in reg._leases.items():
        out.append({"sid": sid, "id": lease.cg_id,
                    "holder": rev.get(lease.holder) if lease.holder is not None else None,
                    "locked": lease.lock.locked(), "pending": lease.pending_acquires,
                    "acquired_at": lease.acquired_at, "last_used": lease.last_used})
    work["leases"] = out
    if token is not None and work.get("token") is not None:
        work["token"]["released"] = token.released
        work["token"]["session_id"] = token.session_id


def d_evict(work, trace, script):
    reg, token, tokens, lease_by_id, rec, armed = _tl_setup(work, trace, script)
    try:
        reg._evict_idle()
        return None
    finally:
        _tl_writeback(work, reg, token, tokens)


def d_rebind(work, trace, script):
    reg, token, tokens, lease_by_id, rec, armed = _tl_setup(work, trace, script)
    for l in work.get("leases", []):
        lease_by_id[l["id"]].lock = LockDouble(l["locked"], script, trace, armed)
    try:
        return reg.rebind(token, work["new_session_id"])
    finally:
        _tl_writeback(work, reg, token, tokens)


def d_release(work, trace, script):
    reg, token, tokens, lease_by_id, rec, armed = _tl_setup(work, trace, script)
    for l in work.get("leases", []):
        lease_by_id[l["id"]].lock = LockDouble(l["locked"], script, trace, armed)
    try:
        return reg.release(token)
    finally:
        _tl_writeback(work, reg, token, tokens)


def d_probe(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_ZAI["httpx"] = types.SimpleNamespace(
        post=rec("httpx.post",
                 lambda a, k: {"url": a[0], "headers": k["headers"], "json": k["json"],
                               "timeout": k["timeout"]},
                 make=lambda r: types.SimpleNamespace(status_code=r["status"])))
    NS_ZAI["logger"] = types.SimpleNamespace(
        debug=rec("logger.debug",
                  lambda a, k: {"fmt": a[0],
                                "args": [str(x) if isinstance(x, BaseException) else x for x in a[1:]]}))
    ep = tuple(work["endpoint"])
    return NS_ZAI["_probe_single_zai_endpoint"](work["api_key"], ep, work["timeout"])


def d_active_count(work, trace, script):
    fence = NS_RET["RetirementFence"]()
    fence._active = work["active"]
    return fence.active_count()


def d_prepare(work, trace, script):
    rec = make_rec(script, trace, [True])
    f = work["fence"]
    fence = NS_RET["RetirementFence"]()
    fence._active = f["active"]
    fence._preparing = f["preparing"]
    fence._token = f["token"]
    fence._committed = f["committed"]
    fence._expires_at = f["expires_at"]
    fence._now = lambda: work["now"]
    mod = types.ModuleType("hermes_cli.web_server_idle_proof")
    mod.idle_proof = rec("idle_proof", lambda a, k: {})
    sys.modules["hermes_cli.web_server_idle_proof"] = mod
    pkg("hermes_cli").web_server_idle_proof = mod
    NS_RET["secrets"] = types.SimpleNamespace(
        token_urlsafe=rec("secrets.token_urlsafe", lambda a, k: {"n": a[0]}))
    NS_RET["logging"] = types.SimpleNamespace(
        getLogger=lambda name: types.SimpleNamespace(
            warning=rec("logging.warning", lambda a, k: {"message": a[0]})))
    try:
        return fence.prepare()
    finally:
        f["active"] = fence._active
        f["preparing"] = fence._preparing
        f["token"] = fence._token
        f["committed"] = fence._committed
        f["expires_at"] = fence._expires_at


def d_custom_entries(work, trace, script):
    rec = make_rec(script, trace, [True], trace_exhaust=True)
    mod = types.ModuleType("hermes_cli.config")
    mod.load_config = rec("load_config", lambda a, k: {})
    mod.get_compatible_custom_providers = rec("get_compatible_custom_providers",
                                              lambda a, k: {"config": a[0]})
    sys.modules["hermes_cli.config"] = mod
    pkg("hermes_cli").config = mod
    return NS_AC["_get_custom_provider_entries"]()


def d_is_current(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_BC["current"] = rec("current", lambda a, k: {})
    return NS_BC["is_current"](work["recorded"])


def d_rotate(work, trace, script):
    rec = make_rec(script, trace, [True])

    class WS:
        def __setattr__(self, name, value):
            if name == "_SESSION_TOKEN":
                r = script.next("set_session_token")
                ev = {"ev": "set_session_token", "args": {"credential": value},
                      "ret": r.spec if isinstance(r, Raise) else norm(r)}
                trace.append(ev)
                if isinstance(r, Raise):
                    raise r.exc
            object.__setattr__(self, name, value)

    ws = WS()
    sys.modules["hermes_cli.web_server"] = ws
    pkg("hermes_cli").web_server = ws
    NS_BC["_revokers"] = {name: rec("revoker", lambda a, k, nm=name: {"name": nm})
                          for name in work.get("revokers", {})}
    return NS_BC["rotate"](work["credential"])


def d_fail(work, trace, script):
    rec = make_rec(script, trace, [True])
    c = types.SimpleNamespace(print=rec("print", lambda a, k: {"message": a[0]}))
    NS_BUN["sys"] = sys
    NS_BUN["_fail"](c, work["message"])
    return None  # live raises SystemExit(1)


def d_confirm(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_CKP["input"] = rec("input", lambda a, k: {"prompt": a[0]})
    NS_CKP["print"] = rec("print", lambda a, k: {"text": " ".join(str(x) for x in a)})
    return NS_CKP["_confirm"](work["prompt"])


def _dock_registry(work, trace, script, rec):
    reg = types.SimpleNamespace(
        list_sessions=rec("registry.list_sessions", lambda a, k: {}),
        get=rec("registry.get", lambda a, k: {"id": a[0]},
                make=lambda r: types.SimpleNamespace(**r) if isinstance(r, dict) else None))
    mod = types.ModuleType("hermes_cli.gateway_tools")
    mod.process_registry = lambda: reg
    sys.modules["hermes_cli.gateway_tools"] = mod
    pkg("hermes_cli").gateway_tools = mod
    return reg


def d_process_rows(work, trace, script):
    rec = make_rec(script, trace, [True])
    _dock_registry(work, trace, script, rec)
    NS_DOCK["time"] = types.SimpleNamespace(time=rec("time.time", lambda a, k: {}))
    NS_DOCK["t"] = rec("t", lambda a, k: {"key": a[0], **k})
    return NS_DOCK["process_rows"](now=work.get("now"))


def d_process_activity(work, trace, script):
    rec = make_rec(script, trace, [True])
    NS_DOCK["t"] = rec("t", lambda a, k: {"key": a[0], **k})
    return NS_DOCK["process_activity"](work["row"])


def d_process_tail(work, trace, script):
    rec = make_rec(script, trace, [True])
    _dock_registry(work, trace, script, rec)
    NS_DOCK["t"] = rec("t", lambda a, k: {"key": a[0], **k})
    return NS_DOCK["process_tail"](work["process_id"])


DRIVERS = {
    "publish_health": d_publish_health,
    "_is_stale": d_is_stale,
    "last_error": d_last_error,
    "lock_for": d_lock_for,
    "_turns": d_turns,
    "take": d_take,
    "filter_and_accumulate": d_filter,
    "flush_think_buffer": d_flush,
    "watchdog_interval_seconds": d_watchdog,
    "lag_tolerance": d_lag_tol,
    "record_tick": d_record_tick,
    "evict_idle": d_evict,
    "rebind": d_rebind,
    "release": d_release,
    "probe_single_zai_endpoint": d_probe,
    "active_count": d_active_count,
    "prepare": d_prepare,
    "get_custom_provider_entries": d_custom_entries,
    "is_current": d_is_current,
    "rotate": d_rotate,
    "_fail": d_fail,
    "_confirm": d_confirm,
    "process_rows": d_process_rows,
    "process_activity": d_process_activity,
    "process_tail": d_process_tail,
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
        spec = {"__raise__": type(e).__name__, "message": str(e)}
        if isinstance(e, SystemExit):
            spec["code"] = e.code
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
    print(f"ORACLE cli_gaps_revive2: {n} cases, {mismatches} mismatches")
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()