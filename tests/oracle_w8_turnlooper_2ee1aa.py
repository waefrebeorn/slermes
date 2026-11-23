#!/usr/bin/env python3
"""Differential oracle for w8_turnlooper_2ee1aa -> agent/turn_loop_errors.py.

Loop-internal collaborators (imported lazily by handle_outer_loop_error) are
registered as scripted stubs reproducing the C port's seams: the error module
sets, _MAX_OUTER_LOOP_ERRORS, shutdown detection, short_detail/site_copy,
append_message and _get_tool_call_name_static.  sys.is_finalizing is patched
per case.  The exception's traceback is a lightweight fake whose frame file
names are the fixture's tb_files.
"""
import importlib.util
import json
import sys
import types
from types import SimpleNamespace

G = "w8_turnlooper_2ee1aa"
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _pkg(name):
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    return m


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


_pkg("agent")


def _site_copy(key, **kwargs):
    return key + "|" + ",".join(f"{k}={v}" for k, v in sorted(kwargs.items()))


_mod("agent.message_metadata", append_message=lambda msgs, m: msgs.append(m))
_mod("agent.turn_failure_copy", short_detail=lambda e: str(e), site_copy=_site_copy)


class _RA:
    class AIAgent:
        @staticmethod
        def _get_tool_call_name_static(tc):
            return tc.get("name", "")


cloop = _mod(
    "agent.conversation_loop",
    _API_CALL_MODULES=frozenset({"api_call", "provider_call", "openai_client"}),
    _LOCAL_PROCESSING_MODULES=frozenset({"message_repair", "sanitize", "local_helpers"}),
    _MAX_OUTER_LOOP_ERRORS=5,
    _is_interpreter_shutdown_error=lambda e: bool(getattr(e, "shutdown", False)),
    _ra=lambda: _RA(),
)


class _FakeTB:
    def __init__(self, filename, nxt=None):
        self.tb_frame = SimpleNamespace(f_code=SimpleNamespace(co_filename=filename))
        self.tb_next = nxt


class _FakeExc:
    def __init__(self, text, files, shutdown):
        self._text = text
        self.shutdown = shutdown
        tb = None
        for f in reversed(files or []):
            tb = _FakeTB(f, tb)
        self.__traceback__ = tb

    def __str__(self):
        return self._text


SRC = Q + "/agent/turn_loop_errors.py"
spec = importlib.util.spec_from_file_location("w8_turnlooper_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w8_turnlooper_live"] = live
spec.loader.exec_module(live)


def canonical(x):
    if x is None or isinstance(x, (bool, str)):
        return x
    if isinstance(x, int):
        return x
    if isinstance(x, float):
        return int(x) if x.is_integer() else x
    if isinstance(x, dict):
        return {k: canonical(v) for k, v in x.items()}
    if isinstance(x, (list, tuple)):
        return [canonical(v) for v in x]
    return x


cases = mism = 0
_real_finalizing = sys.is_finalizing
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") != "handle":
        continue
    cloop._MAX_OUTER_LOOP_ERRORS = int(r.get("max_outer_errors", 5))
    e = _FakeExc(r.get("e_text", ""), r.get("tb_files") or [],
                 bool(r.get("shutdown_error", False)))
    msgs = json.loads(json.dumps(r.get("messages") or []))
    agent = SimpleNamespace(
        _safe_print=lambda *a, **k: None,
        _persist_session=lambda *a, **k: None,
        session_id=r.get("session_id"),
        suppress_status_output=bool(r.get("suppress_status_output", False)),
        max_iterations=int(r.get("max_iterations", 0)))
    sys.is_finalizing = lambda: bool(r.get("is_finalizing", False))
    try:
        v = live.handle_outer_loop_error(
            agent, e=e,
            _outer_error_count=int(r.get("outer_error_count", 0)),
            api_call_count=int(r.get("api_call_count", 0)),
            messages=msgs, conversation_history=[],
            _turn_exit_reason=r.get("turn_exit_reason"),
            failed=bool(r.get("failed", False)),
            final_response=r.get("final_response"))
    except Exception as exc:
        sys.is_finalizing = _real_finalizing
        print(f"LIVE ERROR case={r.get('case')}: {exc!r}", file=sys.stderr)
        mism += 1
        continue
    finally:
        sys.is_finalizing = _real_finalizing
    py = {"action": v.action, "_outer_error_count": v._outer_error_count,
          "_turn_exit_reason": v._turn_exit_reason, "failed": v.failed,
          "final_response": v.final_response, "messages": msgs}
    got = canonical(r.get("out"))
    cases += 1
    if canonical(py) != got:
        mism += 1
        print(f"MISMATCH case={r.get('case')} py={canonical(py)!r} c={got!r}",
              file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or not cases else 0)
