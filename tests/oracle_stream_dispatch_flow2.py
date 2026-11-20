#!/usr/bin/env python3
"""Oracle for the gateway/stream_dispatch.py FLOW remainder
(src/gateway/stream_dispatch_flow2.c).

AST-extracts the LIVE GatewayEventDispatcher methods (dispatch / _dispatch /
_dispatch_tool_call) and the real frozen stream-event dataclasses (no heavy
gateway import), then drives them on a per-case self double whose collaborator
calls (adapter.render_message_event / adapter.format_tool_event, the
_on_long_tool / _on_notice / _enqueue_tool_line callbacks, logger.debug) are
recording stubs driven by the SAME per-name scripts the C harness feeds its
universal seams (in.io). Compares the ordered seam-call trace plus the return
value and the mutated dispatcher state (_last_tool).

Claim accounting: _dispatch_tool_call is the one genuinely UNCLAIMED method
(the legacy stub port claims __init__ / dispatch / _dispatch). dispatch and
_dispatch are exercised here as demoted internal helpers (no new PoP claim).

Script shapes (both sides): a bare JSON array is a SEQUENCE (one result per
call, last element repeats after exhaustion); any other value is reused; a
scripted null is None. FAIL-CLOSED: a missing or empty script raises
RuntimeError("no script for <name>"). BaseException kinds (SystemExit /
KeyboardInterrupt / CancelledError / GeneratorExit) pass through every
`except Exception` site; the drive wrapper catches BaseException into a
{"__raise__": kind, "msg": str(exc)} ret spec.

Modeling: events are tagged "_type" (dataclass name) and materialized into the
real frozen dataclasses so isinstance works; seam args project the event back
to the fixture dict. Dispatcher state (in.state -> out.state) is normalized
like __init__ (tool_mode or "all", preview_max_len default 40) and _last_tool
is the mutable "new"-mode dedup token. Callback/sink presence is an `is None`
identity test (a present marker means installed). out.state echoes
{tool_mode, preview_max_len, _last_tool}. Integral floats normalize to ints.

Input: stdin JSON lines from t_port_stream_dispatch_flow2.
Summary: ORACLE stream_dispatch_flow2: N cases, M mismatches. Fail-closed on 0.
"""
import ast
import asyncio
import json
import os
import sys
import textwrap
import types
import __future__
from dataclasses import dataclass, field
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

QUARRY = r"C:\Users\eman5\hermes-agent"

CUR = {"func": None, "in": {}, "trace": [], "idx": {}}


def make_exc(v):
    kind = v.get("__raise__")
    msg = v.get("msg", "")
    if kind == "CancelledError":
        return asyncio.CancelledError(msg)
    if kind == "SystemExit":
        return SystemExit(msg)
    if kind == "KeyboardInterrupt":
        return KeyboardInterrupt(msg)
    if kind == "GeneratorExit":
        return GeneratorExit(msg)
    builtins = {
        "TypeError": TypeError, "ValueError": ValueError, "AttributeError": AttributeError,
        "KeyError": KeyError, "OSError": OSError, "FileNotFoundError": FileNotFoundError,
        "RuntimeError": RuntimeError, "TimeoutError": TimeoutError, "ImportError": ImportError,
        "OverflowError": OverflowError, "IndexError": IndexError, "Exception": Exception,
        "StopIteration": StopIteration, "ZeroDivisionError": ZeroDivisionError,
        "NotImplementedError": NotImplementedError, "UnicodeError": UnicodeError,
        "UnicodeDecodeError": UnicodeDecodeError, "UnicodeEncodeError": UnicodeEncodeError,
        "UnicodeTranslateError": UnicodeTranslateError,
    }
    cls = builtins.get(kind)
    if cls is None:
        cls = type(str(kind), (Exception,), {})
    # The three Unicode*Error builtins take 5 ctor args; bypass __init__ so a
    # scripted message-only raise reaches the flow (skill trap).
    if kind in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
        e = cls.__new__(cls)
        Exception.__init__(e, msg)
        return e
    return cls(msg)


def maybe_raise(r):
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r)
    return r


def raise_spec(e):
    return {"__raise__": type(e).__name__, "msg": str(e)}


def seam(name, args):
    args = {k: v for k, v in args.items() if v is not None}
    io = CUR["in"].get("io") or {}
    # Distinguish a MISSING script (fail-closed) from a scripted null (None).
    if name not in io:
        r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
    else:
        sc = io[name]
        if isinstance(sc, list):
            if not sc:
                r = {"__raise__": "RuntimeError", "msg": "no script for " + name}
            else:
                i = CUR["idx"].get(name, 0)
                CUR["idx"][name] = i + 1
                r = sc[min(i, len(sc) - 1)]
        else:
            r = sc
    CUR["trace"].append({"ev": name, "args": args, "ret": r})
    return maybe_raise(r)


def norm_nums(x):
    if isinstance(x, float) and x.is_integer():
        return int(x)
    if isinstance(x, dict):
        return {k: norm_nums(v) for k, v in x.items()}
    if isinstance(x, list):
        return [norm_nums(v) for v in x]
    return x


class RecLogger:
    def _rec(self, name, fmt, a, k):
        args = {"fmt": fmt}
        if a:
            args["args"] = [raise_spec(x) if isinstance(x, BaseException) else x for x in a]
        if k.get("exc_info"):
            args["exc_info"] = True
        return maybe_raise(seam(name, args))

    def debug(self, fmt, *a, **k):
        return self._rec("logger.debug", fmt, a, k)

    def info(self, fmt, *a, **k):
        return self._rec("logger.info", fmt, a, k)

    def warning(self, fmt, *a, **k):
        return self._rec("logger.warning", fmt, a, k)

    def error(self, fmt, *a, **k):
        return self._rec("logger.error", fmt, a, k)


def canon(x):
    if isinstance(x, dict):
        return ("obj", sorted(((k, canon(v)) for k, v in x.items()), key=lambda kv: kv[0]))
    if isinstance(x, (list, tuple)):
        return ("arr", [canon(v) for v in x])
    if isinstance(x, bool):
        return ("bool", x)
    if isinstance(x, (int, float)):
        return ("num", float(x))
    if x is None:
        return ("null",)
    return ("str", x)


# ── live-def extraction ──

def _read(rel):
    return open(os.path.join(QUARRY, rel), encoding="utf-8").read()


def extract_events():
    """Exec the tiny stream_events.py module body -> real frozen dataclasses.
    Register the module in sys.modules so dataclass annotation resolution
    (KW_ONLY / _is_type) can find cls.__module__."""
    src = _read("gateway/stream_events.py")
    mod = types.ModuleType("sd2_stream_events")
    mod.__dict__["dataclass"] = dataclass
    mod.__dict__["field"] = field
    sys.modules["sd2_stream_events"] = mod
    exec(compile(src, "gateway/stream_events.py", "exec"), mod.__dict__)
    return mod.__dict__


EVENT_NS = extract_events()
EVENT_CLASSES = {
    "MessageChunk": EVENT_NS["MessageChunk"],
    "MessageStop": EVENT_NS["MessageStop"],
    "Commentary": EVENT_NS["Commentary"],
    "ToolCallChunk": EVENT_NS["ToolCallChunk"],
    "ToolCallFinished": EVENT_NS["ToolCallFinished"],
    "LongToolHint": EVENT_NS["LongToolHint"],
    "GatewayNotice": EVENT_NS["GatewayNotice"],
}

WANTED = ("dispatch", "_dispatch", "_dispatch_tool_call")


def extract_methods():
    src = _read("gateway/stream_dispatch.py")
    tree = ast.parse(src)
    ns = {"__name__": "sd2_stream_dispatch", "__file__": "gateway/stream_dispatch.py"}
    ns.update(EVENT_CLASSES)
    ns["StreamEvent"] = EVENT_NS.get("StreamEvent")
    ns["logger"] = RecLogger()
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "GatewayEventDispatcher")
    flags = __future__.annotations.compiler_flag
    for node in cls.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED:
            seg = textwrap.dedent(ast.get_source_segment(src, node))
            exec(compile(seg, "gateway/stream_dispatch.py", "exec", flags=flags), ns)
    for name in WANTED:
        if name not in ns:
            raise RuntimeError("failed to extract method %s" % name)
    return ns


NS = extract_methods()
NS["logger"] = RecLogger()


def make_event(d):
    if not isinstance(d, dict):
        return object()
    kind = d.get("_type")
    cls = EVENT_CLASSES.get(kind)
    if cls is None:
        return object()  # unknown kind: matches no isinstance branch
    fields = {k: v for k, v in d.items() if k != "_type"}
    return cls(**fields)


def ev_proj():
    return CUR["in"].get("event")


def sink_proj():
    return (CUR["in"].get("state") or {}).get("sink")


def _fmt_tool_event(event, *, mode, preview_max_len):
    return seam("format_tool_event",
                {"event": ev_proj(), "mode": mode, "preview_max_len": preview_max_len})


def _render_message_event(event, sink):
    return seam("render_message_event", {"event": ev_proj(), "sink": sink_proj()})


def _on_long_tool(event):
    return seam("_on_long_tool", {"event": ev_proj()})


def _on_notice(event):
    return seam("_on_notice", {"event": ev_proj()})


def _enqueue_tool_line(line):
    return seam("_enqueue_tool_line", {"line": line})


def build_self(inp):
    st = dict(inp.get("state") or {})
    slf = types.SimpleNamespace()
    slf.tool_mode = st.get("tool_mode") or "all"
    slf.preview_max_len = st.get("preview_max_len", 40)
    slf._last_tool = st.get("_last_tool")
    slf.sink = st.get("sink")
    slf.adapter = types.SimpleNamespace(
        render_message_event=_render_message_event,
        format_tool_event=_fmt_tool_event)
    slf._enqueue_tool_line = _enqueue_tool_line if st.get("_enqueue_tool_line") is not None else None
    slf._on_long_tool = _on_long_tool if st.get("_on_long_tool") is not None else None
    slf._on_notice = _on_notice if st.get("_on_notice") is not None else None
    # Bind the extracted instance methods.
    slf.dispatch = types.MethodType(NS["dispatch"], slf)
    slf._dispatch = types.MethodType(NS["_dispatch"], slf)
    slf._dispatch_tool_call = types.MethodType(NS["_dispatch_tool_call"], slf)
    return slf


def echo_state(slf):
    return {"tool_mode": slf.tool_mode,
            "preview_max_len": slf.preview_max_len,
            "_last_tool": slf._last_tool}


FUNC_TO_METHOD = {
    "dispatch": "dispatch",
    "dispatch_inner": "_dispatch",
    "dispatch_tool_call": "_dispatch_tool_call",
}


def drive(func, inp):
    CUR["idx"] = {}
    NS["logger"] = RecLogger()
    method = FUNC_TO_METHOD.get(func)
    if method is None:
        raise ValueError("unknown func: %s" % func)
    slf = build_self(inp)
    ev = make_event(inp.get("event"))
    fn = getattr(slf, method)
    try:
        ret = fn(ev)
        return {"ret": ret, "state": echo_state(slf)}
    except BaseException as e:
        return {"ret": raise_spec(e), "state": echo_state(slf)}


def main():
    cases = 0
    mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            continue
        func = record.get("func")
        inp = norm_nums(record.get("in") or {})
        got = record.get("out") or {}
        cases += 1
        CUR["func"] = func
        CUR["in"] = inp
        CUR["trace"] = []
        try:
            want = drive(func, inp)
        except BaseException as e:
            want = {"ret": raise_spec(e), "state": None}
        want["trace"] = CUR["trace"]
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE stream_dispatch_flow2: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())