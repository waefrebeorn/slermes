#!/usr/bin/env python3
"""Oracle for the hermes_cli/cli_tui_mixin.py TUI-dispatch FLOW port
(src/cli/port_tui_dispatch_flow.c).

Drives the LIVE CLITuiMixin dispatch methods with SCRIPTED I/O: every
terminal / agent / queue / config / i18n dependency is a recording stub fed
the same scripts the C harness feeds its seams. Compares the DECISION
SEQUENCE (ordered {ev, args, ret} seam events in "trace"), the return
("ret") and the mutated CLI-instance / module-global mirror ("state").
Ported sibling flows are SEAMED on both sides (each is exercised by its own
cases); the real module leaf _tn and the real staticmethod _tui_set_filter
stay LIVE so nested decision logic is exercised, matching the C port's
inline transcriptions.

Live defs are AST-extracted (module Assign/AnnAssign constants + class
MethodDef exec with the future-annotations flag); the heavy quarry import
chain is never imported — in-function imports resolve against stub modules
whose __getattr__ models a scripted import failure (records nothing, raises).
Fail-closed: 0 cases => exit 1. Summary:
'ORACLE tui_dispatch_flow: N cases, M mismatches'.

Representation contract (both sides):
- queues ride as lists; threading.Event as {id, set}; the agent as a spec
  dict with has_steer / has_redirect / _supports_active_turn_redirect flags.
- tuples normalize to lists; integral floats to ints; paths to posix strings.
- an uncaught exception rides as {"__raise__": {type, message}}.
- scripted numbers are integral; _clarify_multi_base / choice values are
  strings; config threshold values are numbers (libjson int/float identity).
- import scripts ("import:<module>" keys) must be SINGLE entries, not queues.
"""
import ast
import copy
import errno
import json
import os
import posixpath
import queue
import shutil
import string
import sys
import threading
import types
import __future__
from pathlib import Path
from typing import Optional

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass  # non-reconfigurable streams (probe kernels); runner pipes are fresh

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "hermes_cli" / "cli_tui_mixin.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "hermes_cli" / "cli_tui_mixin.py")

PY = {
    "handle_enter": "_tui_handle_enter",
    "submit_buffer": "_tui_submit_buffer",
    "enter_inline_command": "_tui_enter_inline_command",
    "enter_while_busy": "_tui_enter_while_busy",
    "enter_overlay": "_tui_enter_overlay",
    "enter_clarify_freetext": "_tui_enter_clarify_freetext",
    "enter_clarify_choice": "_tui_enter_clarify_choice",
    "handle_ctrl_c": "_tui_handle_ctrl_c",
    "handle_ctrl_q": "_tui_handle_ctrl_q",
    "handle_escape_modal": "_tui_handle_escape_modal",
    "handle_ctrl_d": "_tui_handle_ctrl_d",
    "clear_or_exit": "_tui_clear_or_exit",
    "clear_blocking_overlays": "_tui_clear_blocking_overlays",
    "cancel_foreground_ui": "_tui_cancel_foreground_ui",
    "connection_move": "_tui_connection_move",
    "model_picker_escape": "_tui_model_picker_escape",
    "paste_over_threshold": "_tui_paste_over_threshold",
    "suppress_closed_loop_errors": "_tui_suppress_closed_loop_errors",
}

# byte-pinned live constants (hermes_cli/cli_render.py via cli.py; agent/onboarding.py)
DIM = "\x1b[2;3m"
RST = "\x1b[0m"
ACCENT_DEFAULT = "\x1b[1;38;2;255;215;0m"
BUSY_FLAG = "busy_input_prompt"

SELF_FIELDS = [
    "agent", "_agent_running", "_should_exit", "_detach_on_exit",
    "_last_ctrl_c_time", "_attached_images", "_pending_input", "_interrupt_queue",
    "_clarify_state", "_clarify_freetext", "_clarify_prefill", "_clarify_multi_base",
    "_sudo_state", "_secret_state", "_approval_state", "_slash_confirm_state",
    "_connection_state", "_model_picker_state", "_command_palette_state",
    "_gateway_command_wait", "busy_input_mode", "config", "_tui_multiline_shortcuts",
]

OSERROR_FAMILY = {
    "OSError", "PermissionError", "FileNotFoundError", "FileExistsError",
    "BlockingIOError", "ChildProcessError", "ConnectionError", "BrokenPipeError",
    "ConnectionAbortedError", "ConnectionRefusedError", "ConnectionResetError",
    "InterruptedError", "IsADirectoryError", "NotADirectoryError",
    "ProcessLookupError", "TimeoutError",
}


def make_exc(spec):
    spec = spec or {}
    t = spec.get("type", "RuntimeError")
    msg = spec.get("message", "")
    if t == "KeyError":
        e = KeyError(msg)
    elif t == "RuntimeError":
        e = RuntimeError(msg)
    elif t == "RecursionError":
        e = RecursionError(msg)
    elif t == "ValueError":
        e = ValueError(msg)
    elif t == "TypeError":
        e = TypeError(msg)
    elif t in OSERROR_FAMILY:
        cls = {"OSError": OSError, "PermissionError": PermissionError,
               "FileNotFoundError": FileNotFoundError, "FileExistsError": FileExistsError,
               "TimeoutError": TimeoutError}.get(t, OSError)
        e = cls(msg)
    else:
        e = type(t, (Exception,), {})(msg)
    if spec.get("errno") is not None:
        try:
            e.errno = spec["errno"]
        except Exception:
            pass
    return e


def exc_spec(e):
    return {"type": type(e).__name__, "message": str(e),
            "errno": getattr(e, "errno", None)}


# ── AST extraction + namespace seeding ───────────────────────────────────
_src_text = Path(SRC).read_text(encoding="utf-8")
_tree = ast.parse(_src_text)
FLAGS = __future__.annotations.compiler_flag
ns = {
    "json": json, "errno": errno, "string": string, "os": os, "sys": sys,
    "queue": queue, "shutil": shutil, "threading": threading,
    "time": types.ModuleType("time"), "Path": Path, "Optional": Optional,
}


def _exec_node(node):
    exec(compile(ast.Module(body=[node], type_ignores=[]), SRC, "exec", flags=FLAGS), ns)


for _node in _tree.body:
    if isinstance(_node, ast.Assign) and all(isinstance(t, ast.Name) for t in _node.targets):
        _exec_node(_node)
    elif isinstance(_node, ast.AnnAssign) and isinstance(_node.target, ast.Name) and _node.value is not None:
        _exec_node(_node)

_wanted = set(PY.values()) | {"_tn", "_tui_set_filter"}
_found = set()
_cls = next(n for n in _tree.body if isinstance(n, ast.ClassDef) and n.name == "CLITuiMixin")
for _node in _cls.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in _wanted:
        _exec_node(_node)
        _found.add(_node.name)
for _node in _tree.body:
    if isinstance(_node, ast.FunctionDef) and _node.name in _wanted:
        _exec_node(_node)
        _found.add(_node.name)
_missing = _wanted - _found
if _missing:
    print(f"ORACLE tui_dispatch_flow: FATAL missing live defs: {sorted(_missing)}")
    sys.exit(1)


# ── doubles ──────────────────────────────────────────────────────────────
class FakePath:
    def __init__(self, s):
        self._s = str(s).replace("\\", "/")

    def __truediv__(self, other):
        return FakePath(posixpath.join(self._s, str(other)))

    def as_posix(self):
        return self._s

    def __str__(self):
        return self._s


class QueueDouble:
    def __init__(self, items, rec, name):
        self.items = items
        self._rec = rec
        self._name = name

    def put(self, payload):
        self._rec(f"{self._name}.put", {"payload": payload}, None)
        self.items.append(payload)


class EventDouble:
    def __init__(self, spec, rec):
        self.spec = spec
        self._rec = rec

    def set(self):
        self._rec("gateway_command_wait.set", {}, None)
        self.spec["set"] = True


class AgentDouble:
    def __init__(self, spec, stub):
        self._spec = spec
        self._supports_active_turn_redirect = spec.get("_supports_active_turn_redirect", False)
        if spec.get("has_steer"):
            self.steer = lambda text: stub("agent.steer", {"text": text})
        if spec.get("has_redirect"):
            self.redirect = lambda text: stub("agent.redirect", {"text": text})

    def __bool__(self):
        return bool(self._spec)


class BufDouble:
    def __init__(self, b, rec):
        self._b = b
        self._rec = rec

    @property
    def text(self):
        return self._b.get("text", "")

    @text.setter
    def text(self, v):
        self._b["text"] = v

    @property
    def cursor_position(self):
        return self._b.get("cursor_position", 0)

    @cursor_position.setter
    def cursor_position(self, v):
        self._b["cursor_position"] = v

    def reset(self, append_to_history=False):
        self._rec("buffer.reset", {"append_to_history": append_to_history}, None)
        self._b["text"] = ""
        self._b["cursor_position"] = 0

    def delete(self):
        self._rec("buffer.delete", {}, None)
        t = self._b.get("text", "")
        ci = self._b.get("cursor_position", 0)
        if 0 <= ci < len(t):
            self._b["text"] = t[:ci] + t[ci + 1:]


class AppDouble:
    def __init__(self, buf, state, rec):
        self.current_buffer = buf
        self._state = state
        self._rec = rec

    def invalidate(self):
        self._rec("app.invalidate", {}, None)

    def exit(self):
        self._rec("app.exit", {}, None)

    @property
    def is_running(self):
        return self._state.get("app_is_running", True)


def norm(o):
    if isinstance(o, QueueDouble):
        return norm(o.items)
    if isinstance(o, EventDouble):
        return norm(o.spec)
    if isinstance(o, AgentDouble):
        return norm(o._spec)
    if isinstance(o, FakePath):
        return o.as_posix()
    if isinstance(o, Exception):
        return exc_spec(o)
    if isinstance(o, tuple):
        return [norm(x) for x in o]
    if isinstance(o, list):
        return [norm(x) for x in o]
    if isinstance(o, dict):
        return {k: norm(v) for k, v in o.items()}
    if isinstance(o, float) and o.is_integer():
        return int(o)
    return o


class ScriptCtx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}

    def next(self, name):
        e = self.io.get(name)
        if e is None:
            return None
        if isinstance(e, list):
            i = self.cursors.get(name, 0)
            if i >= len(e):
                return None
            self.cursors[name] = i + 1
            return e[i]
        return e


TEXT_FNS = {"_looks_like_slash_command", "_is_backslash_line_continuation",
            "_apply_backslash_line_continuation"}


def run_case(func, inp):
    state = copy.deepcopy(inp.get("state") or {})
    trace = []
    sc = ScriptCtx(inp.get("io"))
    holder = {}

    def rec(name, args, ret):
        trace.append({"ev": name, "args": norm(args), "ret": norm(copy.deepcopy(ret))})

    def wrap_field(k, v):
        if k in ("_pending_input", "_interrupt_queue") and isinstance(v, list):
            return QueueDouble(v, rec, "pending_input" if k == "_pending_input" else "interrupt_queue")
        if k == "_gateway_command_wait" and isinstance(v, dict):
            return EventDouble(v, rec)
        if k == "agent" and isinstance(v, dict):
            return AgentDouble(v, stub)
        if k == "_sudo_state" and isinstance(v, dict) and isinstance(v.get("response_queue"), list):
            v = dict(v)
            v["response_queue"] = QueueDouble(v["response_queue"], rec, "sudo_response_queue")
            return v
        return v

    def apply_effects(eff):
        sd = holder.get("sd")
        for k, v in (eff or {}).items():
            v = copy.deepcopy(v)
            if k in SELF_FIELDS:
                w = wrap_field(k, v)
                if sd is not None:
                    setattr(sd, k, w)
                state[k] = w
            else:
                state[k] = v

    def stub(name, args):
        e = sc.next(name)
        if e is None:
            rec(name, args, None)
            return None
        if isinstance(e, dict):
            apply_effects(e.get("state"))
            if isinstance(e.get("import_raise"), dict):
                raise make_exc(e["import_raise"])
            if isinstance(e.get("raise"), dict):
                rec(name, args, {"__raise__": e["raise"]})
                raise make_exc(e["raise"])
            ret = copy.deepcopy(e.get("ret"))
            rec(name, args, ret)
            return ret
        rec(name, args, e)
        return copy.deepcopy(e)

    # buffer / app / event doubles
    buf = BufDouble(state.setdefault("buf", {"text": "", "cursor_position": 0}), rec)
    app = AppDouble(buf, state, rec)
    event = types.SimpleNamespace(app=app, current_buffer=buf, arg=1)

    # self double: state fields + seamed method stubs
    sd = types.SimpleNamespace()
    for k in SELF_FIELDS:
        setattr(sd, k, wrap_field(k, state.get(k)))
    holder["sd"] = sd

    sd._tui_cancel_voice_recording = lambda event: stub("self._tui_cancel_voice_recording", {})
    sd._tui_cancel_foreground_ui = lambda event, closers: stub(
        "self._tui_cancel_foreground_ui", {"closers": [sa for sa, _ in closers]})
    sd._tui_clear_blocking_overlays = lambda event: stub("self._tui_clear_blocking_overlays", {})
    sd._tui_clear_or_exit = lambda event: stub("self._tui_clear_or_exit", {})
    sd._tui_enter_overlay = lambda event: stub("self._tui_enter_overlay", {})
    sd._tui_submit_buffer = lambda event, b: stub("self._tui_submit_buffer", {"text": b.text})
    sd._tui_enter_inline_command = lambda event, text, has_images: stub(
        "self._tui_enter_inline_command", {"text": text, "has_images": has_images})
    sd._tui_enter_while_busy = lambda text, images, payload: stub(
        "self._tui_enter_while_busy", {"text": text, "images": images, "payload": payload})
    sd._tui_enter_clarify_freetext = lambda event: stub("self._tui_enter_clarify_freetext", {})
    sd._tui_enter_clarify_choice = lambda event: stub("self._tui_enter_clarify_choice", {})
    sd._should_handle_model_command_inline = lambda text, has_images: stub(
        "self._should_handle_model_command_inline", {"text": text, "has_images": has_images})
    sd._should_handle_steer_command_inline = lambda text, has_images: stub(
        "self._should_handle_steer_command_inline", {"text": text, "has_images": has_images})
    sd._should_handle_background_command_inline = lambda text, has_images: stub(
        "self._should_handle_background_command_inline", {"text": text, "has_images": has_images})
    sd._should_handle_stop_command_inline = lambda text, has_images: stub(
        "self._should_handle_stop_command_inline", {"text": text, "has_images": has_images})
    sd.process_command = lambda text: stub("self.process_command", {"text": text})
    sd._clear_active_overlays_for_interrupt = lambda: stub("self._clear_active_overlays_for_interrupt", {})
    sd._connection_cancel = lambda: stub("self._connection_cancel", {})
    sd._cancel_secret_capture = lambda: stub("self._cancel_secret_capture", {})
    sd._submit_secret_response = lambda value: stub("self._submit_secret_response", {"value": value})
    sd._submit_slash_confirm_response = lambda choice: stub(
        "self._submit_slash_confirm_response", {"choice": choice})
    sd._connection_set_field = lambda value: stub("self._connection_set_field", {"value": value})
    sd._connection_submit = lambda: stub("self._connection_submit", {})
    sd._handle_approval_selection = lambda: stub("self._handle_approval_selection", {})
    sd._normalize_slash_confirm_choice = lambda text, choices: stub(
        "self._normalize_slash_confirm_choice", {"text": text, "choices": choices})
    sd._handle_model_picker_selection = lambda persist_global: stub(
        "self._handle_model_picker_selection", {"persist_global": persist_global})
    sd._close_model_picker = lambda: stub("self._close_model_picker", {})
    sd._close_command_palette = lambda: stub("self._close_command_palette", {})
    sd._clarify_batch_lock = lambda st, answer, meta=None: stub(
        "self._clarify_batch_lock", {"state": st, "answer": answer, "meta": meta})
    sd._clarify_batch_enter = lambda st: stub("self._clarify_batch_enter", {"state": st})
    sd._connection_sync_input_buffer = lambda: stub("self._connection_sync_input_buffer", {})
    sd._paint_now = lambda: stub("self._paint_now", {})
    sd._gateway_queue_store = lambda: stub("self._gateway_queue_store", {})
    sd._queue_enqueue = lambda text: stub("self._queue_enqueue", {"text": text})
    sd._inline_pastes = lambda b: stub("self._inline_pastes", {"text": b.text})
    # real staticmethod (no self binding through an instance attribute)
    sd._tui_set_filter = ns["_tui_set_filter"]

    # stub modules for the in-function imports (import failure = untraced raise)
    def import_check(key):
        e = sc.next(key)
        if isinstance(e, dict) and isinstance(e.get("import_raise"), dict):
            raise make_exc(e["import_raise"])

    cli_mod = types.ModuleType("cli")

    def cli_getattr(name):
        import_check("import:cli")
        if name == "CLI_CONFIG":
            return state["CLI_CONFIG"]
        if name == "_hermes_home":
            return FakePath(state.get("_hermes_home", ""))
        if name == "_ACCENT":
            return state.get("_accent", ACCENT_DEFAULT)
        if name == "_DIM":
            return DIM
        if name == "_RST":
            return RST
        if name == "_cprint":
            return lambda text: stub("_cprint", {"text": text})
        if name in TEXT_FNS:
            return lambda text, _n=name: stub(_n, {"text": text})
        raise AttributeError(name)

    cli_mod.__getattr__ = cli_getattr

    hc_pkg = types.ModuleType("hermes_cli")
    hc_pkg.__path__ = []
    burst_mod = types.ModuleType("hermes_cli.cli_enter_burst")

    def burst_getattr(name):
        import_check("import:hermes_cli.cli_enter_burst")
        if name == "handle_burst_enter":
            return lambda self_, ev, b: stub("handle_burst_enter", {"text": b.text})
        raise AttributeError(name)

    burst_mod.__getattr__ = burst_getattr
    ms_mod = types.ModuleType("hermes_cli.model_switch")

    def ms_getattr(name):
        import_check("import:hermes_cli.model_switch")
        if name == "resolve_persist_behavior":
            return lambda a, b: stub("resolve_persist_behavior", {"arg0": a, "arg1": b})
        raise AttributeError(name)

    ms_mod.__getattr__ = ms_getattr
    ag_pkg = types.ModuleType("agent")
    ag_pkg.__path__ = []
    onb_mod = types.ModuleType("agent.onboarding")

    def onb_getattr(name):
        import_check("import:agent.onboarding")
        if name == "BUSY_INPUT_FLAG":
            return BUSY_FLAG
        if name == "is_seen":
            return lambda config, flag: stub("is_seen", {"config": config, "flag": flag})
        if name == "busy_input_hint_cli":
            return lambda mode: stub("busy_input_hint_cli", {"mode": mode})
        if name == "mark_seen":
            return lambda path, flag: stub("mark_seen", {"path": path.as_posix(), "flag": flag})
        raise AttributeError(name)

    onb_mod.__getattr__ = onb_getattr
    sys.modules["cli"] = cli_mod
    sys.modules["hermes_cli"] = hc_pkg
    sys.modules["hermes_cli.cli_enter_burst"] = burst_mod
    sys.modules["hermes_cli.model_switch"] = ms_mod
    sys.modules["agent"] = ag_pkg
    sys.modules["agent.onboarding"] = onb_mod

    # module-level names the extracted defs read (rebound per case)
    ns["t"] = lambda key, **kw: stub("t", {"key": key, **{
        k: (str(v) if isinstance(v, Exception) else v) for k, v in kw.items()}})
    ns["print"] = lambda text: stub("print", {"text": text})
    ns["request_hard_interrupt"] = lambda agent: stub("request_hard_interrupt", {"agent": agent})
    ns["time"].time = lambda: stub("time.time", {})

    f = ns[PY[func]]
    try:
        if func == "enter_inline_command":
            ret = f(sd, event, inp.get("text", ""), inp.get("has_images", False))
        elif func == "enter_while_busy":
            ret = f(sd, inp.get("text", ""), inp.get("images") or [],
                    inp.get("payload"), inp.get("mode"))
        elif func == "cancel_foreground_ui":
            closers = []
            for e in inp.get("closers") or []:
                sa, close = e["state_attr"], e["close"]
                cargs = e.get("args") or {}
                if close == "gateway_command_wait.set":
                    gw = getattr(sd, "_gateway_command_wait", None)
                    fn = gw.set if gw is not None else (lambda: None)
                else:
                    fn = lambda _c=close, _a=cargs: stub(_c, dict(_a))
                closers.append((sa, fn))
            ret = f(sd, event, closers=tuple(closers))
        elif func == "connection_move":
            ret = f(sd, event, inp.get("delta", 0))
        elif func == "paste_over_threshold":
            ret = f(sd, inp.get("text", ""), inp.get("line_count", 0),
                    inp.get("threshold_key", "paste_collapse_threshold"))
        elif func == "suppress_closed_loop_errors":
            context = {}
            for k, v in (inp.get("context") or {}).items():
                context[k] = make_exc(v) if (k == "exception" and isinstance(v, dict)) else copy.deepcopy(v)
            loop = types.SimpleNamespace(
                default_exception_handler=lambda ctx: stub("loop.default_exception_handler", {"context": ctx}))
            ret = f(sd, loop, context)
        else:
            ret = f(sd, event)
        ret = norm(ret)
    except Exception as e:
        ret = {"__raise__": {"type": type(e).__name__, "message": str(e)}}

    for k in SELF_FIELDS:
        state[k] = getattr(sd, k)
    return {"ret": ret, "trace": trace, "state": norm(state)}


def diff_detail(got, want):
    parts = []
    for k in ("ret", "trace", "state"):
        g, w = got.get(k), want.get(k)
        if g == w:
            continue
        if k == "trace" and isinstance(g, list) and isinstance(w, list):
            i = next((j for j in range(max(len(g), len(w)))
                      if (g[j] if j < len(g) else None) != (w[j] if j < len(w) else None)), 0)
            gj = json.dumps(g[i] if i < len(g) else None, ensure_ascii=False)[:280]
            wj = json.dumps(w[i] if i < len(w) else None, ensure_ascii=False)[:280]
            parts.append(f"trace[{i}] C={gj} PY={wj}")
        else:
            parts.append(f"{k} C={json.dumps(g, ensure_ascii=False)[:280]} "
                         f"PY={json.dumps(w, ensure_ascii=False)[:280]}")
    return "; ".join(parts) or "(unknown)"


def main():
    n = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        func = rec.get("func", "")
        inp = rec.get("in") or {}
        got = rec.get("out")
        n += 1
        try:
            want = run_case(func, inp)
        except Exception as e:
            want = {"error": f"{type(e).__name__}: {e}"}
        if got != want:
            mism += 1
            if mism <= 5:
                print(f"MISMATCH {func} case#{n}: {diff_detail(got or {}, want)}")
    if n == 0:
        print("ORACLE tui_dispatch_flow: 0 cases (fail-closed)")
        sys.exit(1)
    print(f"ORACLE tui_dispatch_flow: {n} cases, {mism} mismatches")
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
