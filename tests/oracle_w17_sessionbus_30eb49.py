#!/usr/bin/env python3
"""Oracle for gate w17_sessionbus_30eb49 (gateway/session_busy_controls.py).

Drives the LIVE functions (AST-extracted) against scripted connection/authority/
runner doubles built from the same `io` JSON the C harness consumes.  Compares
canonical `out` = {"ret": ..., "state": ...}; `state` records all observable
seam calls (with every argument) and the mutated live-session fields.
"""
import __future__
import ast
import asyncio as _real_asyncio
import builtins
import contextlib
import json
import sys
import types
from pathlib import Path

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "gateway" / "session_busy_controls.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
sys.path.insert(0, str(QUARRY))
SRC = str(QUARRY / "gateway" / "session_busy_controls.py")
NAME = "w17_sessionbus_30eb49"
PORTED = ["handlers", "authorize", "busy_config", "verbose_config", "show_reasoning_config",
          "service_tier_config", "yolo_config", "adopts_owner_agent", "correct"]

class AuthorityRefusal(Exception):
    def __init__(self, reason=None, **kw):
        super().__init__(reason or "")
        self.reason = reason
MUTATION_GUARD_REFUSALS = (AuthorityRefusal,)

def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m

# ---- parent-package bootstrap + sys.modules registration -------------------
for _pn in ("gateway", "hermes_cli", "agent", "tools", "persistence", "persistence.core"):
    if _pn not in sys.modules:
        _m = types.ModuleType(_pn)
        _m.__path__ = []
        sys.modules[_pn] = _m
sys.modules["persistence.core"].errors = _mod("persistence.core.errors", AuthorityRefusal=AuthorityRefusal)
# placeholder attributes; rebound per case in setup()
_hc_profiles = _mod("hermes_cli.profiles", profile_matches_home=lambda *a, **k: True)
_gw_policy = _mod("gateway.session_policy", policy_for_source=lambda *a, **k: None)
_gw_lve = _mod("gateway.local_viewer_events", launch_shows_reasoning=lambda *a, **k: False)
_gw_mw = _mod("gateway.session_managed_worker", managed_policy=lambda *a, **k: None)
_ag_fast = _mod("agent.fast_mode", parse_exact_service_tier=lambda v: None)
_to_appr = _mod("tools.approval", is_session_yolo_enabled=lambda r: False)
_to_yolo = _mod("tools.approval_yolo", restore_gateway_yolo=lambda *a, **k: None,
                toggle_session_yolo=lambda *a, **k: False)

_ns = {
    "__name__": "w17_sessionbus_live",
    "__builtins__": builtins,
    "AuthorityRefusal": AuthorityRefusal,
    "asyncio": _real_asyncio,
    "partial": __import__("functools").partial,
    "Path": Path,
    "_VERBOSE_CYCLE": ("off", "new", "all", "verbose"),
    "_YOLO_WORDS": {"1": True, "on": True, "true": True,
                    "0": False, "off": False, "false": False},
}
_mod_live = types.ModuleType("w17_sessionbus_live")
_mod_live.__dict__.update(_ns)
sys.modules["w17_sessionbus_live"] = _mod_live
_ns = _mod_live.__dict__

_tree = ast.parse(Path(SRC).read_text(encoding="utf-8"), SRC)
_flags = __future__.annotations.compiler_flag
_seen = set()
for _node in _tree.body:
    if isinstance(_node, (ast.FunctionDef, ast.AsyncFunctionDef)) and _node.name in PORTED:
        exec(compile(ast.Module(body=[_node], type_ignores=[]), SRC, "exec",
                     flags=_flags, dont_inherit=True), _ns)
        _seen.add(_node.name)
missing = set(PORTED) - _seen
if missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(missing)}", file=sys.stderr)
    sys.exit(1)

# ---- doubles ---------------------------------------------------------------
def raise_or(spec, exc_cls):
    if isinstance(spec, dict) and "$raise" in spec:
        raise exc_cls(spec.get("reason", "refused"))
    return spec

def make_live(d, state, io):
    e = types.SimpleNamespace()
    for k in ("source", "route", "tool_progress_mode", "show_reasoning", "busy_input_mode"):
        if k in d:
            setattr(e, k, d[k])
    def publish(sid, payload, event_type=None):
        state.setdefault("publish_calls", []).append(
            {"session_id": sid, "payload": payload, "event_type": event_type})
        return None
    e.event_stream = types.SimpleNamespace(lock=contextlib.nullcontext(), publish=publish)
    return e

def make_connection(r, io, state):
    conn_r = r.get("connection") or {}
    ad = conn_r.get("authority") or {}
    authority = types.SimpleNamespace()
    authority.epoch = ad.get("epoch")
    authority.profile_id = ad.get("profile_id")
    authority.sessions = {sid: make_live(v, state, io)
                          for sid, v in (ad.get("sessions") or {}).items()}
    authority._managed_workers = ad.get("_managed_workers") or []

    def authorize(actor, ref, capability):
        state.setdefault("authorize_calls", []).append(
            {"actor": actor, "ref": ref.session_id, "capability": capability})
        raise_or(io.get("authorize"), AuthorityRefusal)

    authority.authorize = authorize

    def check_generation(sid, generation):
        state.setdefault("check_generation_calls", []).append(
            {"session_id": sid, "generation": generation})
        raise_or(io.get("check_generation"), AuthorityRefusal)

    def steer_agent(ref, generation, text):
        state.setdefault("steer_calls", []).append(
            {"session_id": ref.session_id, "generation": generation, "text": text})
        sa = io.get("steer_agent") or {}
        latched = bool(sa.get("latched"))
        agent = None
        if sa.get("agent"):
            def make_method(verb):
                def method(t):
                    state.setdefault("agent_method_calls", []).append({"text": t})
                    return bool(io.get("agent_accept", False))
                return method
            agent = types.SimpleNamespace(steer=make_method("steer"),
                                          redirect=make_method("redirect"))
        return agent, latched

    authority.check_approval_generation = check_generation
    authority.steer_agent = steer_agent

    def set_tier(route, tier):
        state.setdefault("set_tier_calls", []).append({"route": route, "tier": tier})
    def eff_mode(source):
        return io.get("effective_mode")
    runner = types.SimpleNamespace(
        _set_session_service_tier_override=set_tier,
        _effective_busy_input_mode=eff_mode,
        session_store=(object() if io.get("has_store") else None),
    )
    entry_yolo = io.get("entry_yolo")
    async def lookup(route):
        return types.SimpleNamespace(yolo=entry_yolo)
    runner.async_session_store = types.SimpleNamespace(lookup_by_session_key=lookup)
    authority.runner = runner

    conn = types.SimpleNamespace(authority=authority, actor=conn_r.get("actor"))
    if io.get("voice_stop"):
        async def voice_stop(ref, params):
            state.setdefault("voice_stop_calls", []).append({"session_id": ref.session_id})
            return True
        conn._typed_voice_stop = voice_stop
    return conn, authority

def setup(io, state):
    def profile_matches_home(p, home):
        return bool(io.get("profile_matches_home", True))
    def launch_shows_reasoning(policy, authority):
        return bool(io.get("launch_shows_reasoning", False))
    def parse_exact_service_tier(v):
        spec = (io.get("service_tier") or {}).get(v)
        if isinstance(spec, dict) and "$raise" in spec:
            raise ValueError(spec.get("reason", "bad"))
        return spec
    def policy_for_source(runner, source):
        p = io.get("policy")
        if p is None:
            return None
        return types.SimpleNamespace(source=p.get("source"), yolo=p.get("yolo", False),
                                     config=lambda: dict(p.get("config") or {}))
    def is_session_yolo_enabled(route):
        return bool(io.get("yolo_enabled", False))
    def restore_gateway_yolo(route, yolo, launch=False):
        state.setdefault("yolo_seams", []).append(
            {"route": route, "entry_yolo": yolo, "launch": launch})
    def toggle_session_yolo(route, value, persisted=False, persist=None):
        state.setdefault("yolo_seams", []).append({"route": route, "value": value})
        return bool(io.get("toggle", False))
    def managed_policy(authority, ref):
        spec = io.get("managed_policy")
        raise_or(spec, AuthorityRefusal)
        return spec

    _hc_profiles.profile_matches_home = profile_matches_home
    _gw_policy.policy_for_source = policy_for_source
    _gw_lve.launch_shows_reasoning = launch_shows_reasoning
    _gw_mw.managed_policy = managed_policy
    _ag_fast.parse_exact_service_tier = parse_exact_service_tier
    _to_appr.is_session_yolo_enabled = is_session_yolo_enabled
    _to_yolo.restore_gateway_yolo = restore_gateway_yolo
    _to_yolo.toggle_session_yolo = toggle_session_yolo

def norm(v):
    if v is None or isinstance(v, (bool, str, int, float)):
        return v
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, AuthorityRefusal):
        return {"__raise__": "AuthorityRefusal", "reason": v.reason}
    return v

def live_state(conn, sid):
    live = conn.authority.sessions.get(sid)
    if live is None:
        return {}
    return {"tool_progress_mode": norm(getattr(live, "tool_progress_mode", None)),
            "show_reasoning": norm(getattr(live, "show_reasoning", None)),
            "busy_input_mode": norm(getattr(live, "busy_input_mode", None))}

def drive(r):
    io = r.get("io") or {}
    state = {}
    setup(io, state)
    conn, authority = make_connection(r, io, state)
    sid = r.get("session_id", "")
    ref = types.SimpleNamespace(session_id=sid)
    params = r.get("params") or {}
    write = bool(r.get("write", False))
    func = r.get("func")

    def do(fn, *a, **k):
        try:
            return norm(fn(*a, **k))
        except AuthorityRefusal as e:
            return {"__raise__": "AuthorityRefusal", "reason": e.reason}

    if func == "handlers":
        ret = sorted(_ns["handlers"](conn).keys())
    elif func == "authorize":
        ret = do(_ns["authorize"], conn, ref, params, r.get("capability", ""))
    elif func == "verbose_config":
        ret = do(_ns["verbose_config"], conn, ref, params, write)
    elif func == "show_reasoning_config":
        ret = do(_ns["show_reasoning_config"], conn, ref, params, write)
    elif func == "service_tier_config":
        ret = do(_ns["service_tier_config"], conn, ref, params)
    elif func == "yolo_config":
        ret = _real_asyncio.run(do_async(_ns["yolo_config"], conn, ref, params, write))
    elif func == "adopts_owner_agent":
        ret = do(_ns["adopts_owner_agent"], authority, ref)
    elif func == "correct":
        ret = _real_asyncio.run(do_async(_ns["correct"], conn, ref, params, verb=r.get("verb", "")))
    elif func == "busy_config":
        ret = _real_asyncio.run(do_async(_ns["busy_config"], conn, ref, params, write=write))
    else:
        raise KeyError(func)

    if func in ("verbose_config", "show_reasoning_config", "busy_config"):
        if not (isinstance(ret, dict) and ret.get("__raise__")):
            state["live"] = live_state(conn, sid)
    return {"ret": ret, "state": norm(state)}

async def do_async(fn, *a, **k):
    try:
        return norm(await fn(*a, **k))
    except AuthorityRefusal as e:
        return {"__raise__": "AuthorityRefusal", "reason": e.reason}

def main():
    n = mism = 0
    shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except Exception:
            continue
        n += 1
        try:
            py = drive(r)
        except BaseException as e:
            py = {"ret": {"__raise__": type(e).__name__, "reason": ""}, "state": {}}
        got = r.get("out")
        if got != py:
            mism += 1
            if shown < 10:
                shown += 1
                print(f"MISMATCH {r.get('func')} case {n}:", file=sys.stderr)
                print(f"  C={json.dumps(got, ensure_ascii=False)[:900]}", file=sys.stderr)
                print(f"  P={json.dumps(py, ensure_ascii=False)[:900]}", file=sys.stderr)
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)

if __name__ == "__main__":
    main()
