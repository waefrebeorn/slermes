#!/usr/bin/env python3
"""Oracle for gate w17_sessiondef_feb26c (gateway/session_deferred_pick.py).

Drives the LIVE deferred-pick functions (AST-extracted) against scripted
authority/runtime/store doubles.  Compares `out` = {"ret":..., "state":...}.
"""
import __future__
import ast
import asyncio as _real_asyncio
import builtins
import json
import logging
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
if not (QUARRY / "gateway" / "session_deferred_pick.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
sys.path.insert(0, str(QUARRY))
SRC = str(QUARRY / "gateway" / "session_deferred_pick.py")
NAME = "w17_sessiondef_feb26c"
PORTED = ["defer_pick", "reask_on_attach", "_commit", "_commit_level", "apply_deferred_pick"]

class AuthorityRefusal(Exception):
    def __init__(self, reason=None, **kw):
        super().__init__(reason or "")
        self.reason = reason
class GuardRefusal(Exception):
    pass
MUTATION_GUARD_REFUSALS = (GuardRefusal,)

logging.disable(logging.CRITICAL)
class _Logger:
    def exception(self, *a, **k): pass
    def warning(self, *a, **k): pass
class DeferredPick:
    def __init__(self, **kw): self.__dict__.update(kw)

def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m

for _pn in ("gateway", "persistence", "persistence.core", "persistence.authority"):
    if _pn not in sys.modules:
        _m = types.ModuleType(_pn)
        _m.__path__ = []
        sys.modules[_pn] = _m
sys.modules["persistence.core"].errors = _mod("persistence.core.errors",
        AuthorityRefusal=AuthorityRefusal, MUTATION_GUARD_REFUSALS=MUTATION_GUARD_REFUSALS)
sys.modules["persistence.authority"].runtime = _mod("persistence.authority.runtime",
        DeferredPick=DeferredPick, mutation_receipt_key=lambda *a, **k: (None, None),
        parse_mutation=lambda *a, **k: None)
_mod("gateway.session_mutation_model", configured_default=lambda a: (None, None),
     is_model_reset=lambda p: False)
_mod("gateway.session_mutations", write_mutation=lambda *a, **k: None)
_sp = _mod("gateway.session_persistence", authority_persistence=lambda a: None)

_STATE = {"io": {}, "state": {}}

def parse_mutation(op, payload):
    spec = _STATE["io"].get("parse_mutation")
    if isinstance(spec, dict) and "$raise" in spec:
        raise (ValueError if spec["$raise"] == "ValueError" else RuntimeError)(spec.get("msg", "bad"))

def mutation_receipt_key(subject, sid, request_id, operation="model", payload=None,
                         expected_revision=None, expected_generation=None):
    rk = _STATE["io"].get("receipt_key") or {}
    return rk.get("key"), rk.get("digest")

def configured_default(authority):
    return (_STATE["io"].get("configured_default"), None)

def is_model_reset(payload):
    return bool(_STATE["io"].get("is_model_reset", False))

ns_globals = {
    "__name__": "w17_sessiondef_live", "__builtins__": builtins,
    "logging": logging, "logger": _Logger(), "asyncio": _real_asyncio,
    "AuthorityRefusal": AuthorityRefusal,
    "MUTATION_GUARD_REFUSALS": MUTATION_GUARD_REFUSALS,
    "DeferredPick": DeferredPick,
    "mutation_receipt_key": mutation_receipt_key,
    "parse_mutation": parse_mutation,
    "configured_default": configured_default,
    "is_model_reset": is_model_reset,
    "authority_persistence": lambda a: _STATE["store"],
}
_rt = sys.modules["persistence.authority.runtime"]
_rt.parse_mutation = parse_mutation
_rt.mutation_receipt_key = mutation_receipt_key
sys.modules["gateway.session_mutation_model"].configured_default = configured_default
sys.modules["gateway.session_mutation_model"].is_model_reset = is_model_reset
_sp.authority_persistence = ns_globals["authority_persistence"]

_live = types.ModuleType("w17_sessiondef_live")
_live.__dict__.update(ns_globals)
sys.modules["w17_sessiondef_live"] = _live
_ns = _live.__dict__

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
def make_store(io, state):
    stored = io.get("stored")
    stored_obj = None
    if isinstance(stored, dict):
        stored_obj = types.SimpleNamespace(stored=stored.get("stored"),
                                           pick=stored.get("pick") or {})
    row = io.get("row") or {}
    current = io.get("current") or {}

    class Runtime:
        def defer_model_pick(self, epoch, key=None, digest=None, session_id=None, pick=None):
            state.setdefault("defer_model_pick_calls", []).append({
                "epoch": epoch, "key": key, "digest": digest,
                "session_id": session_id, "pick": pick})
            return io.get("defer_result")
        def has_mutation_receipt(self, principal_id, sid, app_id):
            state.setdefault("has_receipt_calls", []).append({
                "principal_id": principal_id, "session_id": sid, "application_id": app_id})
            return bool(io.get("has_receipt", False))
        def deferred_pick(self, sid):
            return stored_obj
        def forget_deferred_pick(self, sid, stored=None):
            state.setdefault("runtime_calls", []).append(
                {"op": "forget_deferred_pick", "stored": stored})
            return None
        def replace_deferred_pick(self, sid, stored=None, pick=None):
            state.setdefault("runtime_calls", []).append(
                {"op": "replace_deferred_pick", "stored": stored, "pick": pick})
            v = io.get("replace_result", stored)
            return v
        def mark_deferred_pick_asked(self, sid, stored=None, confirm=None):
            state.setdefault("runtime_calls", []).append(
                {"op": "mark_deferred_pick_asked", "stored": stored, "confirm": confirm})
            return None
    class Sessions:
        def get(self, sid):
            return types.SimpleNamespace(
                runtime_revision=row.get("runtime_revision"),
                runtime_generation=row.get("runtime_generation"),
                model=current.get("model"),
                model_config=current.get("model_config") or {})
    class Authority:
        def _schedule(self, ref):
            state.setdefault("schedule_calls", []).append({"session_id": ref.session_id})
    return types.SimpleNamespace(runtime=Runtime(), sessions=Sessions(), authority=Authority())

def make_write_mutation(io, state):
    async def write_mutation(authority, ref, params, _l, principal_id=None):
        state.setdefault("write_mutation_calls", []).append(
            {"params": params, "principal_id": principal_id})
        wr = io.get("write_result")
        if isinstance(wr, dict) and "$raise" in wr:
            cls = wr["$raise"]
            if cls == "GuardRefusal": raise GuardRefusal()
            if cls == "AuthorityRefusal": raise AuthorityRefusal(wr.get("reason", "refused"))
            raise RuntimeError(wr.get("msg", "failed"))
        return wr
    return write_mutation

def norm(v):
    if v is None or isinstance(v, (bool, str, int, float)):
        return v
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, (types.SimpleNamespace, DeferredPick)):
        return {k: norm(x) for k, x in vars(v).items() if not callable(x)}
    if isinstance(v, AuthorityRefusal):
        return {"__raise__": "AuthorityRefusal", "reason": v.reason}
    return v

def live_state(live):
    return {"reask_pick": bool(getattr(live, "reask_pick", False))}

def drive(r):
    io = r.get("io") or {}
    state = {}
    _STATE["io"] = io
    _STATE["state"] = state
    store = make_store(io, state)
    _STATE["store"] = store
    sys.modules["gateway.session_mutations"].write_mutation = make_write_mutation(io, state)

    authority = store.authority
    authority.epoch = (r.get("authority") or {}).get("epoch")
    actor = types.SimpleNamespace(subject=(r.get("actor") or {}).get("subject"))
    sid = r.get("session_id", "")
    ref = types.SimpleNamespace(session_id=sid)
    func = r.get("func")
    ret = None

    def do(fn, *a, **k):
        try:
            return norm(fn(*a, **k))
        except AuthorityRefusal as e:
            return {"__raise__": "AuthorityRefusal", "reason": e.reason}
        except BaseException as e:
            return {"__raise__": type(e).__name__}

    async def do_async(fn, *a, **k):
        try:
            return norm(await fn(*a, **k))
        except AuthorityRefusal as e:
            return {"__raise__": "AuthorityRefusal", "reason": e.reason}
        except BaseException as e:
            return {"__raise__": type(e).__name__}

    try:
        if func == "defer_pick":
            ret = do(_ns["defer_pick"], authority, actor, ref, r.get("params") or {})
        elif func == "reask_on_attach":
            live = types.SimpleNamespace(reask_pick=bool((r.get("live") or {}).get("reask_pick", False)))
            async def _f():
                return await _ns["reask_on_attach"](authority, ref, live)
            _real_asyncio.run(_f())
            ret = None
            state["live"] = live_state(live)
        elif func == "commit":
            ret = _real_asyncio.run(do_async(_ns["_commit"], authority, ref, r.get("entry") or {}, r.get("operation", "model")))
        elif func == "commit_level":
            pick = dict(r.get("pick") or {})
            ret = _real_asyncio.run(do_async(_ns["_commit_level"], authority, ref, r.get("raw"), pick))
        elif func == "apply_deferred_pick":
            def publish(sid, payload, event_type=None):
                state.setdefault("publish_calls", []).append(
                    {"session_id": sid, "payload": payload, "event_type": event_type})
            live = types.SimpleNamespace(
                reask_pick=bool((r.get("live") or {}).get("reask_pick", False)),
                event_stream=types.SimpleNamespace(publish=publish))
            async def _f():
                return await _ns["apply_deferred_pick"](authority, ref, live)
            _real_asyncio.run(_f())
            ret = None
            if any(k in state for k in ("has_receipt_calls", "write_mutation_calls",
                                        "runtime_calls", "publish_calls")):
                state["live"] = live_state(live)
        else:
            raise KeyError(func)
    except BaseException as e:
        ret = {"__raise__": type(e).__name__}
    return {"ret": ret, "state": norm(state)}

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
            py = {"ret": {"__raise__": type(e).__name__}, "state": {}}
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
