#!/usr/bin/env python3
"""Oracle for the flow2 remainder port (src/gateway/tui_methods_voice_flow2.c).

Drives the LIVE bodies of 9 unclaimed functions of
tui_gateway/methods_voice.py (beyond the 27 claimed by tui_methods_voice_flow)
with SCRIPTED seams: every external call, import and claimed flow1 sibling is a
recording stub fed the SAME scripts the C harness feeds its universal seam.
Compares the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in
"trace"), the final return ("ret") and the module-global mirror ("state").

Live defs are AST-extracted from methods_voice.py (module constants seeded,
undecorated FunctionDefs exec'd with the future-annotations flag into a
registered sys.modules module). The heavy quarry import chain is never imported.

Fail-closed: missing defs or 0 cases => exit 2; a missing/exhausted seam script
is a recorded RuntimeError raise on BOTH sides. Per-case wrapper catches
BaseException (SystemExit included). Summary:
'ORACLE tui_methods_voice_flow2: N cases, M mismatches'.
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
    "_frame_fields", "register", "_vr_on_status", "_vr_on_silent_limit",
    "_vr_on_stop_phrase", "_stop_session_voice_capture",
    "_arm_full_duplex_listener", "_wake_probe", "_voice_toggle_mode",
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


def proj(v):
    return v


# -- canonical projection (compare parsed JSON, never raw strings) --
def canon(v):
    def conv(x):
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


# -- per-case script backend (mirrors C tmv2_call exactly) --
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
ARGS_FN = {
    # deferred imports (via ModDouble)
    "stop_continuous": lambda: {},
    "voice_stop_hint": lambda: {},
    "resolve_capture_mode": lambda cfg, prefer_client=False: {
        "cfg": cfg, "prefer_client": prefer_client},
    "check_wake_word_requirements": lambda cfg: {"cfg": cfg},
    # direct seams (claimed flow1 siblings + server globals)
    "voice_emit": lambda event, payload=None: {"event": event, "payload": payload},
    "resume_voice_wake": lambda: {},
    "set_session_voice_off": lambda session_id: {"session_id": session_id},
    "vr_transcript": lambda payload: {"payload": payload},
    "end_session_voice_chat": lambda sid, stop_loop=False, stop_tts=False: {
        "session_id": sid, "stop_loop": stop_loop, "stop_tts": stop_tts},
    "thread_start": lambda target, daemon=False, name="": {
        "target": target if isinstance(target, str) else getattr(target, "__name__", str(target)),
        "daemon": daemon, "name": name},
    "voice_tts_enabled": lambda: {},
    "tts_lease_async": lambda lease, active: {"lease": lease, "active": active},
    "set_voice_tts": lambda on: {"on": on},
    "voice_status_payload": lambda **extra: dict(extra),
    "ok": lambda rid, result: {"rid": rid, "result": result},
    "bind_module": lambda g, s, skip=(): {"server": s, "skip": list(skip)},
}


class ModDouble:
    def __init__(self, ctx, name):
        self.__tmv_module__ = name
        self._ctx = ctx

    def __getattr__(self, attr):
        return rec(self._ctx, attr, ARGS_FN.get(attr))


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


class ThreadDouble:
    def __init__(self, ctx, **kw):
        self._ctx = ctx
        self._kw = kw

    def start(self):
        self._ctx.call("thread_start", ARGS_FN["thread_start"](**self._kw))


def fake_import(name, globals=None, locals=None, fromlist=(), level=0):
    ctx = NS["__tmv_ctx__"]
    ctx.call("import", {"module": name, "fromlist": list(fromlist)})
    return ModDouble(ctx, name)


_BI = dict(vars(builtins))
_BI["__import__"] = fake_import


# -- extraction --
def extract():
    mod = types.ModuleType("tmv2_oracle_methods_voice")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    ns["__builtins__"] = _BI
    src = open(SRC, encoding="utf-8").read()
    seed_constants(src, ns, SRC)
    for node in ast.parse(src).body:
        if isinstance(node, ast.FunctionDef) and not node.decorator_list and node.name in WANTED:
            exec_node(src, node, ns, SRC)
    missing = [k for k in WANTED if k not in ns]
    if missing:
        print(f"FATAL oracle_tui_methods_voice_flow2: methods_voice missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()


STATE_KEYS = ["voice_capture_sid", "voice_capture_gen", "fd_listener_active"]
STATE_DEFAULTS = {
    "voice_capture_sid": "", "voice_capture_gen": 0, "fd_listener_active": False,
}


def _full_duplex_listener():
    pass


def drive(func, inp):
    args = inp.get("args") or {}
    ctx = Ctx(inp.get("io"))
    st = inp.get("state") or {}
    NS["__tmv_ctx__"] = ctx
    for k in STATE_KEYS:
        NS["_" + k] = st.get(k, STATE_DEFAULTS[k])
    NS["_voice_sid_lock"] = NoopLock()
    NS["_fd_listener_lock"] = NoopLock()
    NS["_full_duplex_listener"] = _full_duplex_listener
    NS["contextlib"] = contextlib
    NS["os"] = types.SimpleNamespace(environ=EnvDouble(ctx))
    NS["logger"] = SilentLogger()
    NS["threading"] = types.SimpleNamespace(Thread=lambda **kw: ThreadDouble(ctx, **kw))
    # direct seam stubs (claimed flow1 siblings + server globals)
    NS["_voice_emit"] = rec(ctx, "voice_emit", ARGS_FN["voice_emit"])
    NS["_resume_voice_wake"] = rec(ctx, "resume_voice_wake", ARGS_FN["resume_voice_wake"])
    NS["_set_session_voice_off"] = rec(ctx, "set_session_voice_off", ARGS_FN["set_session_voice_off"])
    NS["_vr_transcript"] = rec(ctx, "vr_transcript", ARGS_FN["vr_transcript"])
    NS["_end_session_voice_chat"] = rec(ctx, "end_session_voice_chat", ARGS_FN["end_session_voice_chat"])
    NS["_voice_tts_enabled"] = rec(ctx, "voice_tts_enabled", ARGS_FN["voice_tts_enabled"])
    NS["_tts_lease_async"] = rec(ctx, "tts_lease_async", ARGS_FN["tts_lease_async"])
    NS["_set_voice_tts"] = rec(ctx, "set_voice_tts", ARGS_FN["set_voice_tts"])
    NS["_voice_status_payload"] = rec(ctx, "voice_status_payload", ARGS_FN["voice_status_payload"])
    NS["_ok"] = rec(ctx, "ok", ARGS_FN["ok"])
    NS["bind_module"] = rec(ctx, "bind_module", ARGS_FN["bind_module"])

    def A(k, default=None):
        return args.get(k, default)

    try:
        if func == "_frame_fields":
            ret = NS[func](A("frame", {}))
        elif func == "register":
            ret = NS[func](A("server"))
        elif func == "_vr_on_status":
            ret = NS[func](A("state"))
        elif func == "_vr_on_silent_limit":
            ret = NS[func]()
        elif func == "_vr_on_stop_phrase":
            ret = NS[func](A("t"))
        elif func == "_stop_session_voice_capture":
            ret = NS[func](A("session_id"))
        elif func == "_arm_full_duplex_listener":
            ret = NS[func]()
        elif func == "_wake_probe":
            ret = NS[func](A("cfg", {}), A("params") or {}, A("surface", ""))
        elif func == "_voice_toggle_mode":
            ret = NS[func](A("rid"), A("params") or {})
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
    "import", "bind_module", "voice_emit", "resume_voice_wake",
    "set_session_voice_off", "vr_transcript", "end_session_voice_chat",
    "stop_continuous", "thread_start", "resolve_capture_mode",
    "check_wake_word_requirements", "env_set", "voice_stop_hint",
    "voice_tts_enabled", "tts_lease_async", "set_voice_tts",
    "voice_status_payload", "ok",
]


def cross_check():
    csrc = (_REPO / "src" / "gateway" / "tui_methods_voice_flow2.c").read_text(encoding="utf-8")
    missing = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing:
        print(f"FATAL oracle_tui_methods_voice_flow2: seams absent from C port: {missing}")
        sys.exit(2)
    c_seams = set(re.findall(r'tmv2_call\(ctx,\s*"([^"]+)"', csrc))
    extra = c_seams - set(SEAM_NAMES)
    if extra:
        print(f"FATAL oracle_tui_methods_voice_flow2: C seams outside registry: {sorted(extra)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "tui_methods_voice_flow2.jsonl"
    if fix.exists():
        keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if line:
                keys |= set((json.loads(line).get("in") or {}).get("io") or {})
        bad = keys - set(SEAM_NAMES)
        if bad:
            print(f"FATAL oracle_tui_methods_voice_flow2: fixture io keys without a seam: {sorted(bad)}")
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
    print(f"ORACLE tui_methods_voice_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    fix = _REPO / "tests" / "fixtures" / "tui_methods_voice_flow2.jsonl"
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
    print(f"SELFTEST tui_methods_voice_flow2: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
