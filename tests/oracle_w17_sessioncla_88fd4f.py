#!/usr/bin/env python3
"""Oracle for gate w17_sessioncla_88fd4f (gateway/session_clarify_batch.py).

Drives the LIVE ask_clarify_batch (AST-extracted) against scripted turn/authority
doubles and records every ask_viewer_result argument in `state`.
"""
import __future__
import ast
import builtins
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
if not (QUARRY / "gateway" / "session_clarify_batch.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
sys.path.insert(0, str(QUARRY))
SRC = str(QUARRY / "gateway" / "session_clarify_batch.py")
NAME = "w17_sessioncla_88fd4f"
PORTED = ["ask_clarify_batch"]

def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m

# ---- parent-package bootstrap + sys.modules registration -------------------
for _pn in ("gateway", "tools"):
    if _pn not in sys.modules:
        _m = types.ModuleType(_pn)
        _m.__path__ = []
        sys.modules[_pn] = _m
_ssr = _mod("gateway.session_server_requests", ask_viewer_result=lambda *a, **k: None)
_cg = _mod("tools.clarify_gateway", get_clarify_timeout=lambda: 0)
_STATE = {"io": {}, "state": {}}

def _ask_viewer_result(event_stream, session_id, kind, params, cancelled=None, timeout=None, qids=None):
    st = _STATE["state"]
    st.setdefault("ask_viewer_calls", []).append({
        "event_stream": event_stream, "session_id": session_id, "kind": kind,
        "params": params, "cancelled": bool(cancelled()) if callable(cancelled) else bool(cancelled),
        "timeout": timeout, "qids": qids})
    return _STATE["io"].get("result")

_ssr.ask_viewer_result = _ask_viewer_result

_ns = {"__name__": "w17_sessioncla_live", "__builtins__": builtins,
       "_QUESTION_KEYS": ("qid", "question", "choices", "multi_select")}
_live = types.ModuleType("w17_sessioncla_live")
_live.__dict__.update(_ns)
sys.modules["w17_sessioncla_live"] = _live
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
    _cg.get_clarify_timeout = lambda: io.get("timeout", 0)

    sessions = {}
    for sid, d in ((r.get("authority") or {}).get("sessions") or {}).items():
        sessions[sid] = types.SimpleNamespace(event_stream=d.get("event_stream"))
    authority = types.SimpleNamespace(sessions=sessions)

    holder_json = ((r.get("turn") or {}).get("ctx") or {}).get("agent_holder")
    holder = []
    if isinstance(holder_json, list) and holder_json:
        if holder_json[0] is None:
            holder.append(None)
        else:
            holder.append(types.SimpleNamespace(
                _interrupt_requested=bool(holder_json[0].get("_interrupt_requested", False))))
    turn = types.SimpleNamespace(
        _approval_owner=(authority, (r.get("turn") or {}).get("approval_owner", [None, "", 0])[1],
                         (r.get("turn") or {}).get("approval_owner", [None, "", 0])[2]),
        _ctx=types.SimpleNamespace(agent_holder=holder))
    ret = _ns["ask_clarify_batch"](turn, r.get("questions") or [])
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
                print(f"  C={json.dumps(got, ensure_ascii=False)[:900]}", file=sys.stderr)
                print(f"  P={json.dumps(py, ensure_ascii=False)[:900]}", file=sys.stderr)
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)

if __name__ == "__main__":
    main()
