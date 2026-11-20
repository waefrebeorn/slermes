#!/usr/bin/env python3
"""Oracle for the 12 remainder FLOW cores of gateway/run_busy.py
(src/gateway/run_busy_flow2.c).

AST-extracts the live module slot helpers and the entire
GatewayBusySessionMixin (future-annotations compile, no heavy gateway import),
then replaces every unported collaborator (t()/i18n, handler getattr,
active_sessions resolvers, session-authority probes, agent adapter send,
logging) with recording stubs fed the SAME scripted results the C harness feeds
its universal seam. Compares the ordered {ev,args,ret} trace + return.

Full builtin exception hierarchy: scripted raises become REAL exceptions via
make_exc (builtins + asyncio.CancelledError); `except Exception` sites catch
only Exception subclasses while BaseException-only kinds propagate exactly as
the C port's parent-chain check does.

Input: stdin JSON lines from t_port_run_busy_flow2 (one {func,in,out} each).
Summary: ORACLE run_busy_flow2: N cases, M mismatches. Fail-closed on 0 cases.
`--shadow` reads tests/fixtures/run_busy_flow2.jsonl and diffs a literal Python
shadow mirror of the C against the live drive (SHADOW ... 0 mismatches).
"""
import ast
import asyncio
import builtins
import contextlib
import functools
import json
import logging
import os
import sys
import time
import types
from types import SimpleNamespace

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

QUARRY = r"C:\Users\eman5\hermes-agent"
SRC = os.path.join(QUARRY, "gateway", "run_busy.py")
FIXTURE = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "fixtures", "run_busy_flow2.jsonl")

WANTED_MOD = {"_strip_slot", "approval_input_words", "_tail_has_slot"}

CUR = {"func": None, "io": {}, "pos": {}, "trace": []}


# ── recording infrastructure ────────────────────────────────────────────────
def rec(ev):
    CUR["trace"].append(ev)


class RecLogger:
    def _rec(self, level, fmt, args):
        rec({"ev": "log", "level": level, "fmt": fmt,
             "args": [str(a) if isinstance(a, BaseException) else a for a in args]})

    def debug(self, fmt, *a, **k): self._rec(10, fmt, a)
    def info(self, fmt, *a, **k): self._rec(20, fmt, a)
    def warning(self, fmt, *a, **k): self._rec(30, fmt, a)
    def error(self, fmt, *a, **k): self._rec(40, fmt, a)


def make_exc(spec):
    """Full builtin exception hierarchy resolution."""
    kind = spec.get("__raise__", "Exception")
    msg = spec.get("message", "")
    cls = getattr(builtins, kind, None)
    if kind == "CancelledError":
        cls = asyncio.CancelledError
    if isinstance(cls, type) and issubclass(cls, BaseException):
        return cls(msg)
    return type(kind, (Exception,), {})(msg)


def next_script(name, optional=False):
    io = CUR["io"]
    if name not in io:
        if optional:
            return None
        raise RuntimeError("no script for " + name)
    v = io[name]
    if isinstance(v, list):
        i = CUR["pos"].get(name, 0)
        if i >= len(v):
            raise RuntimeError("script exhausted for " + name)
        CUR["pos"][name] = i + 1
        return v[i]
    if isinstance(v, dict) and v.get("__script__"):
        return v.get("ret")
    return v


def seam_rec(name, args, optional=False):
    r = next_script(name, optional)
    rec({"ev": name, "args": args, "ret": r})
    if isinstance(r, dict) and "__raise__" in r:
        raise make_exc(r)
    return r


def drop_none(d):
    return {k: v for k, v in d.items() if v is not None}


def mat(x):
    if isinstance(x, dict):
        return SimpleNamespace(**{k: mat(v) for k, v in x.items()})
    if isinstance(x, list):
        return [mat(v) for v in x]
    if isinstance(x, tuple):
        return tuple(mat(v) for v in x)
    return x


def norm(x):
    if isinstance(x, (list, tuple)):
        return [norm(v) for v in x]
    if isinstance(x, dict):
        return {k: norm(v) for k, v in x.items()}
    return x


# ── extraction ──────────────────────────────────────────────────────────────
def extract():
    src = open(SRC, encoding="utf-8").read()
    mod = types.ModuleType("run_busy_flow2_extracted")
    sys.modules[mod.__name__] = mod
    ns = mod.__dict__
    ns.update({
        "logger": RecLogger(),
        "contextlib": contextlib, "asyncio": asyncio, "os": os, "time": time,
        "json": json, "functools": functools, "logging": logging,
        "DEFAULT_LANGUAGE": "en",
        "Platform": SimpleNamespace(TELEGRAM="telegram", WHATSAPP="whatsapp",
                                    DISCORD="discord", SLACK="slack", MATRIX="matrix"),
        "t": lambda key, **kw: key,
        "MessageEvent": lambda **kw: SimpleNamespace(**kw),
    })
    tree = ast.parse(src)
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANTED_MOD:
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec"), ns)
    import __future__
    flags = __future__.annotations.compiler_flag
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == "GatewayBusySessionMixin":
            exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec",
                         flags=flags), ns)
    for name in ("approval_input_words", "_tail_has_slot", "GatewayBusySessionMixin"):
        assert name in ns, "extraction missed %s" % name
    return ns


def install_fake_modules():
    cli = types.ModuleType("hermes_cli")
    active = types.ModuleType("hermes_cli.active_sessions")

    def resolve(config):
        return seam_rec("resolve_max_concurrent_sessions", {"config": norm(config)})

    def limit(ac, mx):
        return seam_rec("active_session_limit_message",
                        {"active_count": ac, "max_sessions": mx})

    active.resolve_max_concurrent_sessions = resolve
    active.active_session_limit_message = limit
    cli.active_sessions = active

    gw = sys.modules.get("gateway") or types.ModuleType("gateway")
    auth = types.ModuleType("gateway.session_authorities")

    def active_authority(runner):
        r = seam_rec("active_authority", {})
        if r is None:
            return None
        return SimpleNamespace(
            route_epoch=lambda key: seam_rec("route_epoch", {"session_key": key}))

    auth.active_authority = active_authority
    gw.session_authorities = auth
    for k, v in (("hermes_cli", cli), ("hermes_cli.active_sessions", active),
                 ("gateway", gw), ("gateway.session_authorities", auth)):
        sys.modules[k] = v


def make_runner(ns, inp):
    Base = ns["GatewayBusySessionMixin"]

    class Runner(Base):
        def __getattr__(self, name):
            # Handler lookup in _command_handler_table / _gateway_idle_*:
            # every missing attribute is the getattr_handler seam.
            return seam_rec("getattr_handler", {"handler_name": name})

    runner = Runner()
    runner._COMMAND_HANDLER_ALIASES = inp.get("aliases", {})
    runner._IDLE_COMMANDS = tuple(inp.get("idle_commands", ()))
    runner.config = inp.get("config")

    def is_running(key):
        return seam_rec("is_session_running", {"session_key": key})
    runner._is_session_running = is_running
    runner._running_agent_count = lambda: seam_rec("running_agent_count", {})

    async def send_busy_reply(event, adapter, message):
        args = drop_none({
            "event_id": event.id if event is not None else None,
            "adapter_id": adapter.id if adapter is not None else None,
            "message": message,
        })
        return seam_rec("send_busy_reply", args)
    runner._send_busy_reply = send_busy_reply
    return runner


def set_t_seam(ns):
    def t(key, **kw):
        return seam_rec("t", drop_none({"key": key, "lang": kw.get("lang")}))
    ns["t"] = t


# ── live drive ──────────────────────────────────────────────────────────────
def drive(ns, func, inp):
    set_t_seam(ns)
    install_fake_modules()
    runner = make_runner(ns, inp)

    if func == "approval_input_words":
        return ns["approval_input_words"](inp.get("input_key", ""))
    if func == "_tail_has_slot":
        return ns["_tail_has_slot"](inp.get("tail", ""), inp.get("slot", ""))
    cls = ns["GatewayBusySessionMixin"]
    if func == "_destructive_optout_note":
        return cls._destructive_optout_note(bool(inp.get("persisted")))
    if func == "_busy_reply_to":
        return cls._busy_reply_to(mat(inp.get("event") or {}), inp.get("reply_anchor"))
    if func == "_demote_interrupt":
        return cls._demote_interrupt(inp.get("session_key", ""), inp.get("why", ""))
    if func == "_command_handler_table":
        return runner._command_handler_table(list(inp.get("names", [])))
    if func == "_gateway_idle_command_handlers":
        return runner._gateway_idle_command_handlers()
    if func == "_plaintext_approval_words":
        return runner._plaintext_approval_words()
    if func == "_get_max_concurrent_sessions":
        return runner._get_max_concurrent_sessions()
    if func == "_active_session_limit_message":
        runner.config = inp.get("config")
        return runner._active_session_limit_message(inp.get("session_key", ""))
    if func == "_route_stop_epoch":
        return runner._route_stop_epoch(inp.get("session_key", ""))
    if func == "_send_busy_ack_reply":
        eid = inp.get("event_id")
        aid = inp.get("adapter_id")
        ev = SimpleNamespace(id=eid) if eid is not None else None
        ad = SimpleNamespace(id=aid) if aid is not None else None
        asyncio.run(runner._send_busy_ack_reply(ev, ad, inp.get("message", "")))
        return None
    raise ValueError("unknown func: %s" % func)


# ── shadow mirror of the C port (for --shadow diff) ─────────────────────────
def _strip_slot_str(text, slot):
    if text == slot:
        return ""
    if text.startswith(slot + ":"):
        return text[len(slot) + 1:]
    return None


def shadow(func, inp):
    """Literal Python translation of the C implementation (incl. trace)."""
    CUR["io"] = inp.get("io") or {}
    CUR["pos"] = {}
    CUR["trace"] = []
    tr = CUR["trace"]

    def call(name, args):
        r = next_script(name)
        rec({"ev": name, "args": args, "ret": r})
        if isinstance(r, dict) and "__raise__" in r:
            return r
        return r

    def log(level, fmt, args):
        rec({"ev": "log", "level": level, "fmt": fmt, "args": args})

    def is_exc_cls(kind):
        m = {
            "ArithmeticError": "Exception", "BufferError": "Exception",
            "EOFError": "Exception", "ImportError": "Exception",
            "LookupError": "Exception", "MemoryError": "Exception",
            "NameError": "Exception", "OSError": "Exception",
            "ReferenceError": "Exception", "RuntimeError": "Exception",
            "StopAsyncIteration": "Exception", "StopIteration": "Exception",
            "SyntaxError": "Exception", "SystemError": "Exception",
            "TypeError": "Exception", "ValueError": "Exception",
            "Warning": "Exception", "AssertionError": "Exception",
            "AttributeError": "Exception",
            "FloatingPointError": "ArithmeticError", "OverflowError": "ArithmeticError",
            "ZeroDivisionError": "ArithmeticError",
            "BlockingIOError": "OSError", "ChildProcessError": "OSError",
            "ConnectionError": "OSError", "FileExistsError": "OSError",
            "FileNotFoundError": "OSError", "InterruptedError": "OSError",
            "IsADirectoryError": "OSError", "NotADirectoryError": "OSError",
            "PermissionError": "OSError", "ProcessLookupError": "OSError",
            "TimeoutError": "OSError",
            "BrokenPipeError": "ConnectionError", "ConnectionAbortedError": "ConnectionError",
            "ConnectionRefusedError": "ConnectionError", "ConnectionResetError": "ConnectionError",
            "ModuleNotFoundError": "ImportError",
            "IndexError": "LookupError", "KeyError": "LookupError",
            "UnboundLocalError": "NameError",
            "NotImplementedError": "RuntimeError", "RecursionError": "RuntimeError",
            "IndentationError": "SyntaxError", "TabError": "IndentationError",
            "UnicodeError": "ValueError", "JSONDecodeError": "ValueError",
            "UnicodeDecodeError": "UnicodeError", "UnicodeEncodeError": "UnicodeError",
            "UnicodeTranslateError": "UnicodeError",
            "BytesWarning": "Warning", "DeprecationWarning": "Warning",
            "FutureWarning": "Warning", "ImportWarning": "Warning",
            "PendingDeprecationWarning": "Warning", "ResourceWarning": "Warning",
            "RuntimeWarning": "Warning", "SyntaxWarning": "Warning",
            "UserWarning": "Warning", "Exception": "BaseException",
            "SystemExit": "BaseException", "KeyboardInterrupt": "BaseException",
            "GeneratorExit": "BaseException", "CancelledError": "BaseException",
        }
        seen = set()
        while kind and kind not in seen:
            if kind == "Exception":
                return True
            seen.add(kind)
            kind = m.get(kind)
        return False

    def is_raise(r):
        return isinstance(r, dict) and isinstance(r.get("__raise__"), str)

    # approval words helper
    def words_key(input_key):
        key = "approval.inputs." + input_key
        out = []
        for call_i in range(2):
            args = {"key": key}
            if call_i == 0:
                args["lang"] = "en"
            r = call("t", args)
            if is_raise(r):
                return ("RAISE", r)
            raw = r if isinstance(r, str) else None
            if raw == key:
                continue
            if raw:
                for w in raw.split(","):
                    w = w.strip().lower()
                    if w and w not in out:
                        out.append(w)
        return ("OK", out)

    if func == "approval_input_words":
        st, v = words_key(inp.get("input_key", ""))
        return {"ret": v, "trace": tr, "raise": v if st == "RAISE" else False}
    if func == "_tail_has_slot":
        r = _strip_slot_str(inp.get("tail", ""), inp.get("slot", ""))
        return {"ret": r is not None, "trace": tr}
    if func == "_destructive_optout_note":
        key = ("gateway.confirm.destructive_optout" if inp.get("persisted")
               else "gateway.confirm.destructive_optout_save_failed")
        r = call("t", {"key": key})
        return {"ret": r, "trace": tr, "raise": r if is_raise(r) else False}
    if func == "_busy_reply_to":
        ev = inp.get("event") or {}
        src = ev.get("source") or {}
        plat = src.get("platform", "")
        tid = src.get("thread_id")
        thread = bool(tid)
        if plat == "telegram" and thread:
            v = inp.get("reply_anchor") if src.get("chat_type") == "dm" else None
            return {"ret": v, "trace": tr}
        if ev.get("internal", False):
            return {"ret": None, "trace": tr}
        return {"ret": ev.get("message_id"), "trace": tr}
    if func == "_demote_interrupt":
        log(20, "Demoting busy_input_mode 'interrupt' to 'queue' for session %s because %s",
            [inp.get("session_key", ""), inp.get("why", "")])
        return {"ret": "queue", "trace": tr}
    if func == "_command_handler_table":
        names = inp.get("names", [])
        aliases = inp.get("aliases", {})
        out = {}
        for name in names:
            hn = aliases.get(name) or ("_handle_" + name.replace("-", "_") + "_command")
            r = call("getattr_handler", {"handler_name": hn})
            if is_raise(r):
                return {"ret": None, "trace": tr, "raise": r}
            out[name] = r
        return {"ret": out, "trace": tr}
    if func == "_gateway_idle_command_handlers":
        names = inp.get("idle_commands", [])
        aliases = inp.get("aliases", {})
        out = {}
        for name in names:
            hn = aliases.get(name) or ("_handle_" + name.replace("-", "_") + "_command")
            r = call("getattr_handler", {"handler_name": hn})
            if is_raise(r):
                return {"ret": None, "trace": tr, "raise": r}
            out[name] = r
        return {"ret": out, "trace": tr}
    if func == "_plaintext_approval_words":
        out = {"👍": ["approve", ""], "👎": ["deny", ""]}
        for i, key in enumerate(["approve", "deny", "always", "session"]):
            st, words = words_key(key)
            if st == "RAISE":
                return {"ret": None, "trace": tr, "raise": words}
            va = [["approve", ""], ["deny", ""], ["approve", "always"],
                  ["approve", "session"]][i]
            for w in words:
                if w not in out:
                    out[w] = va
        return {"ret": out, "trace": tr}
    if func == "_get_max_concurrent_sessions":
        r = call("resolve_max_concurrent_sessions", {"config": inp.get("config")})
        if is_raise(r):
            if is_exc_cls(r["__raise__"]):
                return {"ret": None, "trace": tr}
            return {"ret": None, "trace": tr, "raise": r}
        return {"ret": r, "trace": tr}
    if func == "_active_session_limit_message":
        key = inp.get("session_key", "")
        mx = call("resolve_max_concurrent_sessions", {"config": inp.get("config")})
        if is_raise(mx):
            if is_exc_cls(mx["__raise__"]):
                return {"ret": None, "trace": tr}
            return {"ret": None, "trace": tr, "raise": mx}
        if mx is None:
            return {"ret": None, "trace": tr}
        running = call("is_session_running", {"session_key": key})
        if is_raise(running):
            return {"ret": None, "trace": tr, "raise": running}
        if bool(running):
            return {"ret": None, "trace": tr}
        ac = call("running_agent_count", {})
        if is_raise(ac):
            return {"ret": None, "trace": tr, "raise": ac}
        ac = ac if isinstance(ac, (int, float)) else 0
        mxv = mx if isinstance(mx, (int, float)) else 0
        if ac < mxv:
            return {"ret": None, "trace": tr}
        res = call("active_session_limit_message", {"active_count": ac, "max_sessions": mxv})
        if is_raise(res):
            return {"ret": None, "trace": tr, "raise": res}
        return {"ret": res, "trace": tr}
    if func == "_route_stop_epoch":
        key = inp.get("session_key", "")
        ar = call("active_authority", {})
        if is_raise(ar):
            return {"ret": None, "trace": tr, "raise": ar}
        if ar is None:
            return {"ret": 0, "trace": tr}
        rr = call("route_epoch", {"session_key": key})
        if is_raise(rr):
            return {"ret": None, "trace": tr, "raise": rr}
        return {"ret": rr if rr is not None else 0, "trace": tr}
    if func == "_send_busy_ack_reply":
        args = {"event_id": inp.get("event_id", "")}
        if inp.get("adapter_id") is not None:
            args["adapter_id"] = inp["adapter_id"]
        args["message"] = inp.get("message", "")
        r = call("send_busy_reply", args)
        if is_raise(r):
            if is_exc_cls(r["__raise__"]):
                log(10, "Failed to send busy-ack: %s", [r.get("message", "")])
                return {"ret": None, "trace": tr}
            return {"ret": None, "trace": tr, "raise": r}
        return {"ret": None, "trace": tr}
    raise ValueError("unknown func: %s" % func)


# ── comparison ──────────────────────────────────────────────────────────────
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


def build_want(ns, func, inp):
    CUR["io"] = inp.get("io") or {}
    CUR["pos"] = {}
    CUR["trace"] = []
    try:
        ret = drive(ns, func, inp)
        want_ret = norm(ret)
    except BaseException as e:
        want_ret = {"__raise__": type(e).__name__, "message": str(e)}
    return {"ret": want_ret, "trace": CUR["trace"]}


def main():
    ns = extract()
    if len(sys.argv) > 1 and sys.argv[1] == "--shadow":
        cases = 0
        mism = 0
        with open(FIXTURE, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line.startswith("{"):
                    continue
                c = json.loads(line)
                func, inp = c["func"], c.get("in") or {}
                cases += 1
                sh = shadow(func, inp)
                want = build_want(ns, func, inp)
                sh_ret = sh["ret"]
                sh_raise = sh.get("raise")
                if isinstance(sh_raise, dict):
                    sh_ret = {"__raise__": sh_raise["__raise__"],
                              "message": sh_raise.get("message", "")}
                got = {"ret": sh_ret, "trace": sh["trace"]}
                if canon(got) != canon(want):
                    mism += 1
                    print("SHADOW MISMATCH %s" % func)
                    print("  SHADOW: %s" % json.dumps(got, ensure_ascii=False))
                    print("  LIVE : %s" % json.dumps(want, ensure_ascii=False))
        print("SHADOW run_busy_flow2: %d cases, %d mismatches" % (cases, mism))
        return 0 if (mism == 0 and cases > 0) else 1

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
        inp = record.get("in") or {}
        got = record.get("out") or {}
        cases += 1
        want = build_want(ns, func, inp)
        if canon(want) != canon(got):
            mism += 1
            print("MISMATCH %s" % func)
            print("  C : %s" % json.dumps(got, ensure_ascii=False))
            print("  PY: %s" % json.dumps(want, ensure_ascii=False, default=str))
    print("ORACLE run_busy_flow2: %d cases, %d mismatches" % (cases, mism))
    return 0 if (mism == 0 and cases > 0) else 1


if __name__ == "__main__":
    sys.exit(main())
