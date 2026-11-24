#!/usr/bin/env python3
"""Oracle for gate w17_sessionacp_b27cf9 (gateway/session_acp_lifecycle.py).

Drives the LIVE functions (AST-extracted) against scripted authority/store
doubles built from the same `io` JSON the C harness consumes.  Compares the
canonical `out` = {"ret": ..., "state": ...}, where `state` records every
bindings.end_idle(...) / sessions.reopen(...) call with ALL of its arguments.
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
if not (QUARRY / "gateway" / "session_acp_lifecycle.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
sys.path.insert(0, str(QUARRY))
SRC = str(QUARRY / "gateway" / "session_acp_lifecycle.py")
NAME = "w17_sessionacp_b27cf9"
PORTED = ["end_idle_acp_session", "end_idle_viewer_session", "reopen_if_reattached", "end_idle_acp_sessions"]

logging.disable(logging.CRITICAL)

class AuthorityRefusal(Exception):
    def __init__(self, reason=None, **kw):
        super().__init__(reason or "")
        self.reason = reason

# ---- parent-package bootstrap + sys.modules registration -------------------
for _pn in ("gateway", "persistence", "persistence.core", "persistence.core.errors"):
    if _pn not in sys.modules:
        _m = types.ModuleType(_pn)
        _m.__path__ = []
        sys.modules[_pn] = _m
_err = sys.modules["persistence.core.errors"]
_err.AuthorityRefusal = AuthorityRefusal
sys.modules["persistence.core"].errors = _err
_gw = sys.modules["gateway"]
if "session_persistence" not in sys.modules:
    _sp = types.ModuleType("gateway.session_persistence")
    sys.modules["gateway.session_persistence"] = _sp
    _gw.session_persistence = _sp

_STATE = {"io": {}, "state": {}, "store": None}

def _authority_persistence(authority):
    return _STATE["store"]

sys.modules["gateway.session_persistence"].authority_persistence = _authority_persistence

_ns = {
    "__name__": "w17_sessionacp_live",
    "__builtins__": builtins,
    "logging": logging,
    "AuthorityRefusal": AuthorityRefusal,
    "authority_persistence": _authority_persistence,
    "ACP_END_REASON": "acp_disconnect",
}
_mod = types.ModuleType("w17_sessionacp_live")
_mod.__dict__.update(_ns)
sys.modules["w17_sessionacp_live"] = _mod
_ns = _mod.__dict__

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
    class Bindings:
        def receipt(self, sid):
            spec = (io.get("receipts") or {}).get(sid)
            if spec is None or (isinstance(spec, dict) and "$raise" in spec):
                raise AuthorityRefusal("not_local")
            return types.SimpleNamespace(policy=spec.get("policy") or {},
                                         entry=spec.get("entry") or {})
        def end_idle(self, **kw):
            state.setdefault("end_idle_calls", []).append({
                "epoch": kw.get("epoch"), "session_id": kw.get("session_id"),
                "target_id": kw.get("target_id"), "reason": kw.get("reason")})
            spec = (io.get("end_idle") or {}).get(kw.get("session_id"))
            if isinstance(spec, dict) and "$raise" in spec:
                raise AuthorityRefusal("end_refused")
            return spec
        def local_session_ids(self):
            return list(io.get("local_session_ids") or [])
    class Sessions:
        def reopen(self, target):
            state.setdefault("reopen_calls", []).append({"target": target})
    return types.SimpleNamespace(bindings=Bindings(), sessions=Sessions())

def make_authority(r):
    sess = {}
    for sid, d in (r.get("authority") or {}).get("sessions", {}).items():
        sess[sid] = types.SimpleNamespace(subscribers=d.get("subscribers") or [])
    return types.SimpleNamespace(sessions=sess, epoch=(r.get("authority") or {}).get("epoch"))

def norm(v):
    if v is None or isinstance(v, (bool, str, int, float)):
        return v
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, types.SimpleNamespace):
        return {k: norm(x) for k, x in vars(v).items() if not callable(x)}
    return v

def drive(r):
    io = r.get("io") or {}
    state = {}
    _STATE["io"] = io
    _STATE["state"] = state
    _STATE["store"] = make_store(io, state)
    authority = make_authority(r)
    func = r.get("func")
    sid = r.get("session_id", "")
    if func == "end_idle_acp_session":
        ret = _ns["end_idle_acp_session"](authority, sid)
    elif func == "end_idle_viewer_session":
        ret = _ns["end_idle_viewer_session"](authority, sid, r.get("reason", ""))
    elif func == "reopen_if_reattached":
        ret = _real_asyncio.run(_ns["reopen_if_reattached"](authority, sid, r.get("ended", False)))
    elif func == "end_idle_acp_sessions":
        ret = _ns["end_idle_acp_sessions"](authority)
    else:
        raise KeyError(func)
    return {"ret": norm(ret), "state": norm(state)}

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
                print(f"  C={json.dumps(got, ensure_ascii=False)[:800]}", file=sys.stderr)
                print(f"  P={json.dumps(py, ensure_ascii=False)[:800]}", file=sys.stderr)
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)

if __name__ == "__main__":
    main()
