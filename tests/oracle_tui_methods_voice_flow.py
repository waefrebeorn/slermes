#!/usr/bin/env python3
"""Oracle for the TUI voice-method FLOW port (src/gateway/tui_methods_voice_flow.c).

Drives the LIVE bodies of 27 UNCLAIMED functions of
tui_gateway/methods_voice.py with SCRIPTED seams: every server global
(_ok/_err/_load_cfg/_emit/bind_transport/...), claimed sibling
(_tts_stream_stop/_tts_lease_async/_arm_full_duplex_listener/...), deferred
import and env read/write is a recording stub fed the SAME scripts the C
harness feeds its universal seam. Compares the DECISION SEQUENCE (ordered
seam events {"name","args","ret"} in "trace"), the final return ("ret") and
the module-global mirror ("state").

Live defs are AST-extracted from methods_voice.py (module Assign/AnnAssign
constants seeded, undecorated FunctionDefs exec'd with the future-annotations
flag into a registered sys.modules module -- py3.14). The heavy quarry import
chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2; a missing/exhausted seam script
is a recorded RuntimeError raise on BOTH sides. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE tui_methods_voice_flow: N cases, M mismatches'.
"""
import ast
import builtins
import contextlib
import copy
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

_REPO = Path(__file__).resolve().parent.parent
Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "tui_gateway", "methods_voice.py")
_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


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


WANTED = [
    "_caller_transport", "_voice_emit", "_resume_voice_wake",
    "_voice_mode_enabled", "_voice_tts_enabled", "_session_voice_enabled",
    "_set_session_voice_off", "_capture_live", "_end_voice_chat",
    "_end_session_voice_chat", "_running_sessions", "_any_session_running",
    "_arm_barge_listener_if_enabled", "_fd_barge_params", "_voice_cfg_dict",
    "_voice_cfg_number", "_voice_status_payload", "_wake_owner_snapshot",
    "_release_wake_for_transport", "_release_gateway_wake_owner",
    "_persist_wake_enabled", "_owner_result", "_deliver_fd_transcript",
    "_voice_toggle_status", "_set_voice_tts", "_voice_toggle_tts",
    "_vr_transcript",
]

# -- exception classes (real builtins; full hierarchy) --
EXC_CLASSES = {
    "BaseException": BaseException, "Exception": Exception,
    "ArithmeticError": ArithmeticError, "AssertionError": AssertionError,
    "AttributeError": AttributeError, "BufferError": BufferError,
    "EOFError": EOFError, "ImportError": ImportError, "LookupError": LookupError,
    "MemoryError": MemoryError, "NameError": NameError, "OSError": OSError,
    "ReferenceError": ReferenceError, "RuntimeError": RuntimeError,
    "StopAsyncIteration": StopAsyncIteration, "StopIteration": StopIteration,
    "SyntaxError": SyntaxError, "SystemError": SystemError, "TypeError": TypeError,
    "ValueError": ValueError, "Warning": Warning,
    "ModuleNotFoundError": ModuleNotFoundError, "UnicodeError": UnicodeError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeTranslateError": UnicodeTranslateError,
    "UnboundLocalError": UnboundLocalError, "RecursionError": RecursionError,
    "NotImplementedError": NotImplementedError, "ZeroDivisionError": ZeroDivisionError,
    "OverflowError": OverflowError, "FloatingPointError": FloatingPointError,
    "IndexError": IndexError, "KeyError": KeyError,
    "BlockingIOError": BlockingIOError, "ChildProcessError": ChildProcessError,
    "ConnectionError": ConnectionError, "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError, "FileExistsError": FileExistsError,
    "FileNotFoundError": FileNotFoundError, "InterruptedError": InterruptedError,
    "IsADirectoryError": IsADirectoryError, "NotADirectoryError": NotADirectoryError,
    "PermissionError": PermissionError, "ProcessLookupError": ProcessLookupError,
    "TimeoutError": TimeoutError, "EnvironmentError": EnvironmentError,
    "IOError": IOError, "WindowsError": OSError,
    "DeprecationWarning": DeprecationWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "PendingDeprecationWarning": PendingDeprecationWarning,
    "ResourceWarning": ResourceWarning, "RuntimeWarning": RuntimeWarning,
    "SyntaxWarning": SyntaxWarning, "UserWarning": UserWarning,
    "BytesWarning": BytesWarning, "EncodingWarning": EncodingWarning,
    "UnicodeWarning": UnicodeWarning, "IndentationError": IndentationError,
    "TabError": TabError, "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind) or type(kind, (Exception,), {})
    msg = spec.get("message", "")
    if kind in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
    else:
        try:
            e = cls(msg)
        except Exception:
            e = cls.__new__(cls)
            Exception.__init__(e, msg)
    e.__tmv_spec__ = {"__raise__": kind, "message": msg}
    return e


def proj_exc(exc):
    spec = getattr(exc, "__tmv_spec__", None)
    if spec is not None:
        return dict(spec)
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# -- transport double (identity via per-case registry) --
class TD:
    def __init__(self, ident):
        self.__tmv_id__ = ident


class Reg:
    def __init__(self):
        self.m = {}

    def t(self, x):
        if x is None:
            return None
        return self.m.setdefault(x, TD(x))


def proj(v):
    if isinstance(v, TD):
        return v.__tmv_id__
    return v


# -- canonical projection (compare parsed JSON, never raw strings) --
def canon(v):
    def conv(x):
        if isinstance(x, TD):
            return x.__tmv_id__
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, Path):
            return x.as_posix()
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, float) and x.is_integer():
            return int(x)
        if isinstance(x, BaseException):
            return str(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


# -- per-case script backend (mirrors C tmv_call exactly) --
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": canon(args), "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        args = {k: v for k, v in (args or {}).items() if v is not None}
        if name not in self.io:            # KEY PRESENCE, not .get() is None
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
        entry = self.io[name]
        used = entry
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
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        return copy.deepcopy(ret) if isinstance(ret, (dict, list)) else ret


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


# -- module double: from X import Y -> import seam + Y's call seam --
ATTR_SEAM = {
    "local_voice_enabled": "local_voice_enabled",
    "set_local_voice_mode": "set_local_voice_mode",
    "stop_continuous": "stop_continuous",
    "is_continuous_active": "is_continuous_active",
    "save_config_value": "save_config_value",
    "stop_listening": "stop_listening",
    "is_voice_stop_phrase": "is_voice_stop_phrase",
    "check_voice_requirements": "check_voice_requirements",
}
ARGS_FN = {
    "local_voice_enabled": lambda session_id: {"session_id": session_id},
    "set_local_voice_mode": lambda session_id, mode: {"session_id": session_id, "mode": mode},
    "stop_continuous": lambda: {},
    "is_continuous_active": lambda: {},
    "save_config_value": lambda key, value: {"key": key, "value": value},
    "stop_listening": lambda owner: {"owner": owner},
    "is_voice_stop_phrase": lambda text: {"text": text},
    "check_voice_requirements": lambda: {},
    "current_transport": lambda: {},
    "bind_transport": lambda transport: {"transport": transport},
    "reset_transport": lambda token: {"token": token},
    "emit": lambda event, sid, payload=None: {"event": event, "sid": sid, "payload": payload},
    "wake_resume_if_owner": lambda owner: {"owner": owner},
    "tts_stream_stop": lambda user_barge=True: {"user_barge": user_barge},
    "tts_lease_async": lambda lease, active: {"lease": lease, "active": active},
    "arm_full_duplex_listener": lambda: {},
    "load_cfg": lambda: {},
    "ok": lambda rid, result: {"rid": rid, "result": result},
    "err": lambda rid, code, message: {"rid": rid, "code": code, "message": message},
}


class ModDouble:
    def __init__(self, ctx, name):
        self.__tmv_module__ = name
        self._ctx = ctx

    def __getattr__(self, attr):
        seam = ATTR_SEAM.get(attr, attr)
        return rec(self._ctx, seam, ARGS_FN.get(seam))


class EnvDouble:
    def __init__(self, ctx):
        self._ctx = ctx

    def get(self, key, default=""):
        return self._ctx.call("env_get", {"key": key, "default": default})

    def __setitem__(self, key, value):
        self._ctx.call("env_set", {"key": key, "value": value})


class NoopLock:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


class SilentLogger:
    def debug(self, *a, **k):
        pass

    def info(self, *a, **k):
        pass

    def warning(self, *a, **k):
        pass

    def exception(self, *a, **k):
        pass


def fake_import(name, globals=None, locals=None, fromlist=(), level=0):
    ctx = NS["__tmv_ctx__"]
    ctx.call("import", {"module": name, "fromlist": list(fromlist)})
    return ModDouble(ctx, name)


_BI = dict(vars(builtins))
_BI["__import__"] = fake_import


# -- extraction --
def extract():
    mod = types.ModuleType("tmv_oracle_methods_voice")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    # Functions capture __builtins__ at creation time: install the import
    # double BEFORE exec'ing any of them.
    ns["__builtins__"] = _BI
    src = open(SRC, encoding="utf-8").read()
    seed_constants(src, ns, SRC)
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and not node.decorator_list and node.name in WANTED:
            exec_node(src, node, ns, SRC)
    missing = [k for k in WANTED if k not in ns]
    if missing:
        print(f"FATAL oracle_tui_methods_voice_flow: methods_voice missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()


STATE_KEYS = [
    "voice_event_sid", "voice_event_transport", "voice_wake_owner",
    "voice_capture_sid", "voice_capture_gen", "wake_owner_transport",
    "wake_owner_surface", "fd_listener_active", "sessions",
]
STATE_DEFAULTS = {
    "voice_event_sid": "", "voice_event_transport": None, "voice_wake_owner": None,
    "voice_capture_sid": "", "voice_capture_gen": 0, "wake_owner_transport": None,
    "wake_owner_surface": "", "fd_listener_active": False, "sessions": {},
}
TRANSPORT_STATE_KEYS = {"voice_event_transport", "voice_wake_owner", "wake_owner_transport"}


def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    reg = Reg()
    st = inp.get("state") or {}
    NS["__tmv_ctx__"] = ctx
    # seed state globals (C state keys omit the leading underscore of the
    # live module globals: voice_event_sid -> _voice_event_sid, sessions->_sessions)
    for k in STATE_KEYS:
        v = st.get(k, STATE_DEFAULTS[k])
        if k in TRANSPORT_STATE_KEYS:
            v = reg.t(v)
        NS["_" + k] = v
    NS["_voice_sid_lock"] = NoopLock()
    NS["_wake_lock"] = NoopLock()
    NS["_sessions_lock"] = NoopLock()
    NS["_fd_listener_lock"] = NoopLock()
    NS["_stdio_transport"] = reg.t("stdio")
    NS["logger"] = SilentLogger()
    NS["contextlib"] = contextlib
    NS["os"] = types.SimpleNamespace(environ=EnvDouble(ctx))
    # stubs
    NS["current_transport"] = rec(ctx, "current_transport", ARGS_FN["current_transport"])
    NS["bind_transport"] = rec(ctx, "bind_transport", ARGS_FN["bind_transport"],
                               lambda raw: reg.t(raw))
    NS["reset_transport"] = rec(ctx, "reset_transport", ARGS_FN["reset_transport"])
    NS["_emit"] = rec(ctx, "emit", ARGS_FN["emit"])
    NS["_wake_resume_if_owner"] = rec(ctx, "wake_resume_if_owner", ARGS_FN["wake_resume_if_owner"])
    NS["_ok"] = rec(ctx, "ok", ARGS_FN["ok"])
    NS["_err"] = rec(ctx, "err", ARGS_FN["err"])
    NS["_load_cfg"] = rec(ctx, "load_cfg", ARGS_FN["load_cfg"])
    NS["_tts_stream_stop"] = rec(ctx, "tts_stream_stop", ARGS_FN["tts_stream_stop"])
    NS["_tts_lease_async"] = rec(ctx, "tts_lease_async", ARGS_FN["tts_lease_async"])
    NS["_arm_full_duplex_listener"] = rec(ctx, "arm_full_duplex_listener",
                                          ARGS_FN["arm_full_duplex_listener"])

    def A(k, default=None):
        return args.get(k, default)

    try:
        if func == "_caller_transport":
            ret = NS[func]()
        elif func == "_voice_emit":
            ret = NS[func](A("event", ""), A("payload"))
        elif func in ("_resume_voice_wake", "_voice_mode_enabled", "_voice_tts_enabled",
                      "_capture_live", "_running_sessions", "_any_session_running",
                      "_arm_barge_listener_if_enabled", "_voice_cfg_dict",
                      "_wake_owner_snapshot", "_release_gateway_wake_owner"):
            ret = NS[func]()
        elif func == "_session_voice_enabled":
            ret = NS[func](A("session_id"))
        elif func == "_set_session_voice_off":
            ret = NS[func](A("session_id"))
        elif func == "_end_voice_chat":
            ret = NS[func](stop_loop=A("stop_loop", False), stop_tts=A("stop_tts", False))
        elif func == "_end_session_voice_chat":
            ret = NS[func](A("session_id"), stop_loop=A("stop_loop", False),
                           stop_tts=A("stop_tts", False))
        elif func == "_fd_barge_params":
            ret = NS[func](args)
        elif func == "_voice_cfg_number":
            ret = NS[func](A("value"), A("default"))
        elif func == "_voice_status_payload":
            ret = NS[func](**(A("extra") or {}))
        elif func == "_release_wake_for_transport":
            ret = NS[func](reg.t(A("transport")))
        elif func == "_persist_wake_enabled":
            ret = NS[func](A("enabled", False))
        elif func == "_owner_result":
            ret = NS[func](A("rid"), A("field", ""), A("ok", False), **(A("extra") or {}))
        elif func == "_deliver_fd_transcript":
            ret = NS[func](A("text", ""))
        elif func == "_voice_toggle_status":
            ret = NS[func](A("rid"), A("params") or {})
        elif func == "_set_voice_tts":
            ret = NS[func](A("on", False))
        elif func == "_voice_toggle_tts":
            ret = NS[func](A("rid"), A("params") or {})
        elif func == "_vr_transcript":
            ret = NS[func](A("payload"))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        ret = proj_exc(exc)

    state_out = {}
    for k in STATE_KEYS:
        state_out[k] = canon(proj(NS.get("_" + k, STATE_DEFAULTS[k])))
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": state_out}


# -- three-way seam-name cross-check --
SEAM_NAMES = [
    "import", "current_transport", "bind_transport", "emit", "reset_transport",
    "wake_resume_if_owner", "env_get", "env_set", "local_voice_enabled",
    "set_local_voice_mode", "stop_continuous", "is_continuous_active",
    "tts_stream_stop", "tts_lease_async", "arm_full_duplex_listener", "load_cfg",
    "ok", "err", "stop_listening", "save_config_value", "is_voice_stop_phrase",
    "check_voice_requirements",
]


def cross_check():
    csrc = ( _REPO / "src" / "gateway" / "tui_methods_voice_flow.c").read_text(encoding="utf-8")
    missing = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing:
        print(f"FATAL oracle_tui_methods_voice_flow: seams absent from C port: {missing}")
        sys.exit(2)
    c_seams = set(re.findall(r'tmv_call\(ctx,\s*"([^"]+)"', csrc))
    extra = c_seams - set(SEAM_NAMES)
    if extra:
        print(f"FATAL oracle_tui_methods_voice_flow: C seams outside registry: {sorted(extra)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "tui_methods_voice_flow.jsonl"
    if fix.exists():
        keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if line:
                keys |= set((json.loads(line).get("in") or {}).get("io") or {})
        bad = keys - set(SEAM_NAMES)
        if bad:
            print(f"FATAL oracle_tui_methods_voice_flow: fixture io keys without a seam: {sorted(bad)}")
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
    if shown >= 14:
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


def main():
    cross_check()
    n = m = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            rec_in = json.loads(line)
        except Exception as exc:
            m += 1
            print(f"MISMATCH <unparsable harness line>: {exc}")
            continue
        n += 1
        func = rec_in.get("func", "")
        inp = rec_in.get("in") or {}
        got = rec_in.get("out")
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
    print(f"ORACLE tui_methods_voice_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "tui_methods_voice_flow.jsonl"
    n = bad = 0
    counts = {}
    for line in open(fix, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            out = drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
            continue
        r = out["ret"]
        if isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            if not case.get("fail_closed"):
                bad += 1
                print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
    thin = [f for f in counts if counts[f] < 5]
    if thin:
        bad += 1
        print(f"SELFTEST THIN COVERAGE (<5 cases): {thin}")
    print(f"SELFTEST tui_methods_voice_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
