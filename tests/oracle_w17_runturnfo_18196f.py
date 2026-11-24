#!/usr/bin/env python3
"""Oracle for gate w17_runturnfo_18196f (gateway/run_turn_followup_ack.py).

Drives the LIVE functions (AST-extracted, annotations-as-strings) with object
doubles built from the same JSON the C harness consumes.  Compares the canonical
`out` field:
  apply    -> bool
  cancel   -> "CANCELLED"/"FAILURE"
  run_hook -> array of recorded hook calls.
"""
import __future__
import ast
import asyncio as _real_asyncio
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
if not (QUARRY / "gateway" / "run_turn_followup_ack.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
sys.path.insert(0, str(QUARRY))
SRC = str(QUARRY / "gateway" / "run_turn_followup_ack.py")
NAME = "w17_runturnfo_18196f"
PORTED = ["_followup_processing_hooks_apply", "_followup_cancel_outcome", "_run_followup_processing_hook"]

# ---- parent-package bootstrap + sys.modules registration -------------------
class BasePlatformAdapter:
    def on_processing_start(self, *a, **k):
        return None

    def on_processing_complete(self, *a, **k):
        return None

class MessageEvent:
    pass

class ProcessingOutcome:
    CANCELLED = "CANCELLED"
    FAILURE = "FAILURE"

for _pn in ("gateway", "gateway.platforms"):
    if _pn not in sys.modules:
        _m = types.ModuleType(_pn)
        _m.__path__ = []
        sys.modules[_pn] = _m
_bp = sys.modules.get("gateway.platforms.base")
if _bp is None:
    _bp = types.ModuleType("gateway.platforms.base")
    sys.modules["gateway.platforms.base"] = _bp
    sys.modules["gateway.platforms"].base = _bp
_bp.BasePlatformAdapter = BasePlatformAdapter
_bp.MessageEvent = MessageEvent
_bp.ProcessingOutcome = ProcessingOutcome

_STATE = {"current_task": None}

class _AsyncioStub:
    @staticmethod
    def current_task():
        cfg = _STATE.get("current_task")
        if isinstance(cfg, dict) and "$raise" in cfg:
            raise RuntimeError("no running event loop")
        return cfg

_ns = {
    "__name__": "w17_runturnfo_live",
    "__builtins__": builtins,
    "BasePlatformAdapter": BasePlatformAdapter,
    "MessageEvent": MessageEvent,
    "ProcessingOutcome": ProcessingOutcome,
    "asyncio": _AsyncioStub(),
}
_mod = types.ModuleType("w17_runturnfo_live")
_mod.__dict__.update(_ns)
sys.modules["w17_runturnfo_live"] = _mod
_ns = _mod.__dict__

_tree = ast.parse(Path(SRC).read_text(encoding="utf-8"), SRC)
_flags = __future__.annotations.compiler_flag
_seen = set()
for _node in _tree.body:
    if isinstance(_node, ast.AsyncFunctionDef) and _node.name in PORTED:
        _node = _node  # keep async
    if isinstance(_node, (ast.FunctionDef, ast.AsyncFunctionDef)) and _node.name in PORTED:
        exec(compile(ast.Module(body=[_node], type_ignores=[]), SRC, "exec",
                     flags=_flags, dont_inherit=True), _ns)
        _seen.add(_node.name)
missing = set(PORTED) - _seen
if missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(missing)}", file=sys.stderr)
    sys.exit(1)

# ---- object doubles ---------------------------------------------------------
def mk_adapter(d, record_hook, event_json, extra):
    if d is None:
        return None
    overrides = bool(d.get("overrides_start", False))
    if overrides:
        class A(BasePlatformAdapter):
            def on_processing_start(self, *a, **k):
                return None
    else:
        class A(BasePlatformAdapter):
            pass
    ad = A()
    if "expected_cancelled" in d:
        ec = d["expected_cancelled"]
        ad._expected_cancelled_tasks = set(ec) if isinstance(ec, list) else ec
    if d.get("run_hook"):
        async def rh(*a, **k):
            # a[0]=hook_name, a[1]=event, a[2:]=extra args
            record_hook.append({"hook": a[0] if len(a) > 0 else None,
                                "event": event_json,
                                "extra": extra})
        ad._run_processing_hook = rh
    return ad

def mk_event(d):
    if d is None:
        return None
    e = types.SimpleNamespace()
    if isinstance(d, dict):
        for k in ("message_id", "raw_message"):
            if k in d:
                setattr(e, k, d[k])
    return e

def drive(r):
    func = r.get("func")
    if func == "apply":
        return _ns["_followup_processing_hooks_apply"](mk_adapter(r.get("adapter"), [], None, []),
                                                       mk_event(r.get("event")))
    if func == "cancel":
        _STATE["current_task"] = (r.get("io") or {}).get("current_task")
        return _ns["_followup_cancel_outcome"](mk_adapter(r.get("adapter"), [], None, []))
    if func == "run_hook":
        hooks = []
        event_json = r.get("event")
        extra = r.get("args") or []
        ad = mk_adapter(r.get("adapter"), hooks, event_json, extra)
        ev = mk_event(event_json)
        _real_asyncio.run(_ns["_run_followup_processing_hook"](ad, ev, r.get("hook_name", ""), *extra))
        return hooks
    raise KeyError(func)

def norm(v):
    if isinstance(v, bool) or v is None or isinstance(v, (str, int, float)):
        return v
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    if isinstance(v, ProcessingOutcome):
        return getattr(v, "value", str(v))
    return v

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
            py = norm(drive(r))
        except BaseException as e:
            py = {"__raise__": type(e).__name__}
        got = r.get("out")
        if got != py:
            mism += 1
            if shown < 10:
                shown += 1
                print(f"MISMATCH {r.get('func')} case {n}:", file=sys.stderr)
                print(f"  C={json.dumps(got, ensure_ascii=False)[:600]}", file=sys.stderr)
                print(f"  P={json.dumps(py, ensure_ascii=False)[:600]}", file=sys.stderr)
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)

if __name__ == "__main__":
    main()
