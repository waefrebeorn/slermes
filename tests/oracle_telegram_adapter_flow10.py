#!/usr/bin/env python3
"""Oracle for the Telegram adapter FLOW part 10 port
(src/gateway/platforms/telegram_adapter_flow10.c).

Drives the LIVE plugins/platforms/telegram/adapter.py surface with SCRIPTED
seams.  Every non-pure collaborator is a recording stub fed the SAME scripts
the C harness feeds its universal seam.  Compares the ordered seam trace +
final return + state echo.

The live defs are AST-extracted from the adapter module/class bodies
(decorators excluded by lineno, segments dedented); the exec namespace is a
registered sys.modules module (py3.14 annotation resolution).  Async defs are
driven with asyncio.run(); awaited collaborators are async recording stubs.

Fail-closed: missing defs or 0 cases => exit 2.  Per-case wrapper catches
BaseException (SystemExit included).  Summary:
'ORACLE telegram_adapter_flow10: N cases, M mismatches'.
"""
import ast
import asyncio
import builtins
import contextlib
import copy
import enum
import json
import os
import re
import sys
import textwrap
import types
from pathlib import Path
from types import SimpleNamespace
import __future__ as _fut

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
A_SRC = os.path.join(Q, "plugins", "platforms", "telegram", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag

KINDS = {
    "_accept_update": "instance",
    "_bot_commands": "module",
    "_await_with_thread_deadline": "module",
    "_bounded_request_step": "instance",
    "_bot_identity_refresh_loop": "instance",
    "_after_legacy_edit": "instance",
    "_attach_cached": "instance",
    "_build_triggered_event": "instance",
    "_note_instead_of_media": "module",
    "_is_connected": "module",
    "_truncate_stream_overflow_preview": "instance",
    "_parse_page": "module",
    "_edit_html_quiet": "module",
    "_build_model_keyboard": "instance",
    "_await_disconnect_step": "instance",
    "_build_provider_keyboard": "instance",
}

# EXACTLY the C ta10_call seam names (set-equality enforced in cross_check).
SEAM_NAMES = [
    "admission_get", "BotCommand", "run_bounded_async", "logger_debug",
    "sleep", "refresh_bot_identity", "quotable_text", "record_sent_text",
    "pad_inline_flags", "append_observed_note", "logger_info",
    "_build_message_event", "group_trigger_text", "_cache_replied_media",
    "_apply_telegram_group_observe_attribution", "add_item_note",
    "get_env_value", "truncate_message", "_toast", "answer_callback",
    "edit_message_text", "_format_choice_page", "model_button_labels",
    "InlineKeyboardButton", "_picker_back_cancel_row", "_paged_keyboard",
    "ensure_future", "wait", "task_cancel", "add_done_callback", "task_await",
    "logger_warning", "group_providers", "_provider_button", "t",
]


def seg(src, node):
    return "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


def extract():
    mod = new_module("ta10_oracle_adapter")
    ns = mod.__dict__
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    found = set()
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in KINDS:
            exec_node(src, node, ns, A_SRC)
            found.add(node.name)
    cls = next(n for n in tree.body
               if isinstance(n, ast.ClassDef) and n.name == "TelegramAdapter")
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in KINDS:
            exec_node(src, node, ns, A_SRC)
            found.add(node.name)
    missing = [k for k in KINDS if k not in ns]
    if missing:
        print(f"FATAL oracle_telegram_adapter_flow10: missing: {missing}")
        sys.exit(2)
    return ns, found


NS, _FOUND = extract()


# ── canonical projection ──
class _Task:
    def __init__(self, raw):
        self._raw = raw

    def cancel(self):
        return _CUR.st["task_cancel"](self)

    def add_done_callback(self, cb):
        return _CUR.st["add_done_callback"](self, cb)

    def __await__(self):
        return _CUR.st["task_await"](self).__await__()


class _Adm:
    def get(self):
        return _CUR.st["admission_get"]()


def canon(v):
    def conv(x):
        if isinstance(x, _Adm):
            return {}
        if isinstance(x, _Task):
            return conv(x._raw)
        if isinstance(x, enum.Enum):
            return x.value
        if isinstance(x, BaseException):
            return {"__raise__": type(x).__name__, "message": str(x)}
        if isinstance(x, types.SimpleNamespace):
            return {k: conv(val) for k, val in vars(x).items() if not callable(val)}
        if isinstance(x, dict):
            return {k: conv(val) for k, val in x.items()}
        if isinstance(x, (list, tuple)):
            return [conv(i) for i in x]
        if isinstance(x, (set, frozenset)):
            return sorted(conv(i) for i in x)
        if isinstance(x, float) and x.is_integer():
            return int(x)
        return x
    return json.loads(json.dumps(conv(v), ensure_ascii=False, default=str))


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = asyncio.CancelledError if kind == "CancelledError" else getattr(builtins, kind, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


# ── per-case script backend (mirrors the C ta10_call exactly) ──
_MISSING = object()
_CUR = None


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []
        self.st = {}
        self.inst = None
        self.task = None
        self.query = None

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": canon(args), "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            self._record_raise(name, args, RuntimeError(f"no script for {name}"))
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
        return ret


def rec(name, args_fn, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k)
        raw = _CUR.call(name, args)
        return build(raw) if build else raw
    return f


def arec(name, args_fn, build=None):
    async def f(*a, **k):
        args = args_fn(*a, **k)
        raw = _CUR.call(name, args)
        return build(raw) if build else raw
    return f


# ── seam stubs ──
def build_seams():
    st = {}
    def _build_claim(raw):
        obj = SimpleNamespace(**raw) if isinstance(raw, dict) else raw
        _CUR.inst._claim = obj
        return obj
    st["admission_get"] = rec("admission_get", lambda: {}, build=_build_claim)
    st["BotCommand"] = rec(
        "BotCommand", lambda name, desc: {"name": name, "description": desc})
    st["run_bounded_async"] = arec(
        "run_bounded_async",
        lambda awaitable, timeout, *, label, on_abandon, dump_on_blocked_loop: {
            "awaitable": awaitable, "timeout": timeout, "label": label,
            "on_abandon": on_abandon, "dump_on_blocked_loop": dump_on_blocked_loop},
        build=lambda raw: SimpleNamespace(**raw) if isinstance(raw, dict) else raw)
    st["logger_debug"] = rec(
        "logger_debug", lambda fmt, name, exc_info=False: {
            "fmt": fmt, "name": name, "exc_info": exc_info})
    st["sleep"] = arec("sleep", lambda seconds: {"seconds": seconds})

    def _mk_task(raw):
        t = _Task(raw)
        _CUR.task = t
        return t
    st["refresh_bot_identity"] = arec(
        "refresh_bot_identity", lambda self_, *, force: {"force": force})
    st["quotable_text"] = rec("quotable_text", lambda content: {"content": content})
    st["record_sent_text"] = arec(
        "record_sent_text",
        lambda self_, chat_id, message_id, text: {
            "chat_id": chat_id, "message_id": message_id, "text": text})
    st["pad_inline_flags"] = rec("pad_inline_flags", lambda event: {"event": event})
    st["append_observed_note"] = rec(
        "append_observed_note", lambda self_, existing, note: {"existing": existing, "note": note})
    st["logger_info"] = rec(
        "logger_info", lambda fmt, kind, path: {"fmt": fmt, "kind": kind, "path": path})

    def _build_event(raw):
        obj = SimpleNamespace(**raw) if isinstance(raw, dict) else raw
        _CUR.inst._event = obj
        return obj
    st["_build_message_event"] = rec(
        "_build_message_event",
        lambda self_, msg, msg_type, update_id: {
            "msg": msg, "msg_type": msg_type, "update_id": update_id},
        build=_build_event)
    st["group_trigger_text"] = rec(
        "group_trigger_text", lambda self_, msg, text: {"msg": msg, "text": text})
    st["_cache_replied_media"] = arec(
        "_cache_replied_media", lambda self_, msg, event: {"msg": msg, "event": event})
    st["_apply_telegram_group_observe_attribution"] = rec(
        "_apply_telegram_group_observe_attribution", lambda self_, event: {"event": event})
    st["add_item_note"] = rec(
        "add_item_note", lambda event, text: {"event": event, "text": text})
    st["get_env_value"] = rec("get_env_value", lambda key: {"key": key})
    st["truncate_message"] = rec(
        "truncate_message",
        lambda self_, content, max_length, len_fn: {
            "content": content, "max_length": max_length,
            "len_fn": getattr(len_fn, "__name__", None)})
    st["_toast"] = rec("_toast", lambda key: {"key": key})
    st["answer_callback"] = arec(
        "answer_callback", lambda query, text: {"query": query, "text": text})
    st["edit_message_text"] = arec(
        "edit_message_text",
        lambda text, parse_mode, reply_markup: {
            "query": _CUR.query, "text": text, "parse_mode": parse_mode,
            "reply_markup": reply_markup})
    st["_format_choice_page"] = rec(
        "_format_choice_page",
        lambda self_, items, page, page_size: {
            "items": items, "page": page, "page_size": page_size})
    st["model_button_labels"] = rec(
        "model_button_labels", lambda labels, suffix_fn: {"labels": labels})
    st["InlineKeyboardButton"] = rec(
        "InlineKeyboardButton",
        lambda text, callback_data: {"text": text, "callback_data": callback_data})
    st["_picker_back_cancel_row"] = rec("_picker_back_cancel_row", lambda self_: {})
    st["_paged_keyboard"] = rec(
        "_paged_keyboard",
        lambda self_, buttons, page_meta, nav_prefix, tail_row, columns=2: {
            "buttons": buttons, "page_meta": page_meta, "nav_prefix": nav_prefix,
            "tail_row": tail_row, "columns": columns})
    st["ensure_future"] = rec(
        "ensure_future", lambda awaitable: {"awaitable": awaitable}, build=_mk_task)
    st["wait"] = arec(
        "wait",
        lambda tasks, timeout: {"tasks": list(tasks), "timeout": timeout},
        build=lambda raw: ({_CUR.task} if raw.get("done") else set(), set()))
    st["task_cancel"] = rec("task_cancel", lambda task: {"task": task})
    st["add_done_callback"] = rec(
        "add_done_callback",
        lambda task, cb: {"task": task, "callback": getattr(cb, "__name__", None)})
    st["task_await"] = arec("task_await", lambda task: {"task": task})
    st["logger_warning"] = rec(
        "logger_warning",
        lambda fmt, name, step, timeout: {
            "fmt": fmt, "name": name, "step": step, "timeout": timeout})
    st["group_providers"] = rec("group_providers", lambda slugs: {"slugs": slugs})
    st["_provider_button"] = rec("_provider_button", lambda self_, p: {"p": p})
    st["t"] = rec("t", lambda key, **kwargs: {"key": key, "kwargs": kwargs})
    return st


# methods installed on the `self` double (class-level, so not in vars())
METHOD_STUBS = {
    "_record_sent_text": "record_sent_text",
    "_append_observed_note": "append_observed_note",
    "_refresh_bot_identity": "refresh_bot_identity",
    "_build_message_event": "_build_message_event",
    "_cache_replied_media": "_cache_replied_media",
    "_apply_telegram_group_observe_attribution": "_apply_telegram_group_observe_attribution",
    "_format_choice_page": "_format_choice_page",
    "_picker_back_cancel_row": "_picker_back_cancel_row",
    "_paged_keyboard": "_paged_keyboard",
    "_provider_button": "_provider_button",
    "truncate_message": "truncate_message",
}


def ns_double(v):
    if isinstance(v, dict):
        return SimpleNamespace(**{k: ns_double(x) for k, x in v.items()})
    if isinstance(v, list):
        return [ns_double(x) for x in v]
    return v


def make_inst(state, st):
    methods = {m: st[seam] for m, seam in METHOD_STUBS.items()}
    cls = type("AdapterDouble", (), methods)
    obj = cls()
    for k, v in (state or {}).items():
        if k == "_update_admission" and isinstance(v, dict):
            setattr(obj, k, _Adm())
        else:
            setattr(obj, k, copy.deepcopy(v))
    return obj


def seed_globals(st, state):
    NS["logger"] = SimpleNamespace(
        info=st["logger_info"], debug=st["logger_debug"],
        warning=st["logger_warning"], error=(lambda *a, **k: None))
    NS["run_bounded_async"] = st["run_bounded_async"]
    NS["t"] = st["t"]
    NS["InlineKeyboardButton"] = st["InlineKeyboardButton"]
    NS["quotable_text"] = st["quotable_text"]
    NS["add_item_note"] = st["add_item_note"]
    NS["answer_callback"] = st["answer_callback"]
    NS["_toast"] = st["_toast"]
    NS["model_button_labels"] = st["model_button_labels"]
    NS["asyncio"] = SimpleNamespace(
        sleep=st["sleep"], ensure_future=st["ensure_future"], wait=st["wait"],
        CancelledError=asyncio.CancelledError, TimeoutError=TimeoutError)
    NS["contextlib"] = contextlib
    NS["ParseMode"] = SimpleNamespace(HTML="HTML", MARKDOWN_V2="MARKDOWN_V2")
    NS["_BOT_COMMAND_DESCRIPTION_LIMIT"] = 256
    NS["_DRAIN_TIMEOUT"] = 15.0

    def utf16_len(s):
        return len(s)
    NS["utf16_len"] = utf16_len

    def _consume_abandoned_task(task):
        return None
    NS["_consume_abandoned_task"] = _consume_abandoned_task


def seed_modules(st, state):
    def mod(name, **attrs):
        m = types.ModuleType(name)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[name] = m
        return m

    # package parents with __path__ for `from pkg.sub import x`
    for pkg in ("plugins", "plugins.platforms", "plugins.platforms.telegram",
                "hermes_cli", "gateway", "gateway.platforms"):
        m = sys.modules.get(pkg) or types.ModuleType(pkg)
        m.__path__ = []
        sys.modules[pkg] = m

    mod("telegram", BotCommand=st["BotCommand"])
    mod("plugins.platforms.telegram.telegram_batching", pad_inline_flags=st["pad_inline_flags"])
    mod("hermes_cli.model_picker_usage", picker_row_suffix=(lambda *a, **k: ""))
    gp = st["group_providers"] if state.get("group_providers_present", True) else None
    mod("hermes_cli.models_catalog_static", group_providers=gp)
    mod("plugins.platforms.telegram.telegram_context",
        group_trigger_text=st["group_trigger_text"],
        group_identity_prompt=(lambda *a, **k: None),
        group_reply_expected=(lambda *a, **k: None))
    mod("gateway.platforms.base", resolve_channel_prompt=(lambda *a, **k: None))
    mod("hermes_cli.gateway", get_env_value=st["get_env_value"])


def run_call(fn, *a, **kw):
    r = fn(*a, **kw)
    if asyncio.iscoroutine(r):
        return asyncio.run(r)
    return r


def drive(func, inp):
    global _CUR
    args = copy.deepcopy(inp.get("args") or {})
    state = copy.deepcopy(inp.get("state") or {})
    ctx = Ctx(inp.get("io"))
    _CUR = ctx
    st = build_seams()
    ctx.st = st
    seed_globals(st, state)
    seed_modules(st, state)
    inst = make_inst(state, st)
    ctx.inst = inst

    def A(k):
        return ns_double(args.get(k))

    try:
        if func == "_accept_update":
            ret = NS[func](inst)
        elif func == "_bot_commands":
            ret = NS[func](args.get("menu_commands"))
        elif func == "_await_with_thread_deadline":
            ret = run_call(NS[func], A("awaitable"), args.get("timeout"),
                           label=args.get("label", "telegram"),
                           on_abandon=A("on_abandon"),
                           dump_on_blocked_loop=args.get("dump_on_blocked_loop", True))
        elif func == "_bounded_request_step":
            ret = run_call(NS[func], inst, A("awaitable"), args.get("failure_msg", ""))
        elif func == "_bot_identity_refresh_loop":
            ret = run_call(NS[func], inst)
        elif func == "_after_legacy_edit":
            ret = run_call(NS[func], inst, args.get("chat_id"), args.get("message_id"),
                           args.get("content"), args.get("plain"),
                           args.get("finalize", False), args.get("saturated_preview", False))
        elif func == "_attach_cached":
            ev = ns_double(args.get("event"))
            inst.event = ev
            ret = NS[func](inst, ev, A("cached"), args.get("note", ""),
                           args.get("log_fmt", ""), retype=args.get("retype", True))
        elif func == "_build_triggered_event":
            ret = run_call(NS[func], inst, A("msg"), A("update"), A("msg_type"))
        elif func == "_note_instead_of_media":
            ret = NS[func](A("event"), args.get("text", ""))
        elif func == "_is_connected":
            ret = NS[func](A("config"))
        elif func == "_truncate_stream_overflow_preview":
            ret = NS[func](inst, args.get("content", ""))
        elif func == "_parse_page":
            ret = run_call(NS[func], A("query"), args.get("raw", ""))
        elif func == "_edit_html_quiet":
            q = A("query")
            ctx.query = q
            if isinstance(q, SimpleNamespace):
                q.edit_message_text = st["edit_message_text"]
            else:
                q = SimpleNamespace(edit_message_text=st["edit_message_text"])
            ret = run_call(NS[func], q, args.get("text", ""))
        elif func == "_build_model_keyboard":
            ret = NS[func](inst, A("models"), args.get("page", 0), A("provider"))
        elif func == "_await_disconnect_step":
            ret = run_call(NS[func], inst, A("awaitable"), args.get("timeout", 0.0),
                           args.get("step", ""))
        elif func == "_build_provider_keyboard":
            ret = NS[func](inst, args.get("providers", []), args.get("page", 0))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(vars(inst))}


# ── cross-checks ──
def cross_check():
    csrc = (_REPO / "src" / "gateway" / "platforms" /
            "telegram_adapter_flow10.c").read_text(encoding="utf-8")
    c_seams = set(re.findall(r'ta10_call\(\s*ctx,\s*"([^"]+)"', csrc))
    if c_seams != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow10: C seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - c_seams)} extra={sorted(c_seams - set(SEAM_NAMES))}")
        sys.exit(2)
    osrc = Path(__file__).read_text(encoding="utf-8")
    used = set(re.findall(r'rec\(\s*"([^"]+)"', osrc)) | set(re.findall(r'arec\(\s*"([^"]+)"', osrc))
    used |= set(re.findall(r'st\["([^"]+)"\]\s*=\s*(?:a?rec)', osrc))
    if used != set(SEAM_NAMES):
        print(f"FATAL oracle_telegram_adapter_flow10: oracle seam set != SEAM_NAMES: "
              f"missing={sorted(set(SEAM_NAMES) - used)} extra={sorted(used - set(SEAM_NAMES))}")
        sys.exit(2)
    pops = set(re.findall(r'PoP:\s*ta10_\w+\s*@\s*plugins/platforms/telegram/adapter\.py:(\w+)', csrc))
    if pops != set(KINDS):
        print(f"FATAL oracle_telegram_adapter_flow10: PoP/KINDS drift: c={sorted(pops)} k={sorted(KINDS)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow10.jsonl"
    if fix.exists():
        fix_keys, fix_funcs = set(), set()
        for line in fix.open(encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            fix_keys |= set((case.get("in") or {}).get("io") or {})
            fix_funcs.add(case.get("func", ""))
        if fix_keys != set(SEAM_NAMES):
            print(f"FATAL oracle_telegram_adapter_flow10: fixture io key set != SEAM_NAMES: "
                  f"missing={sorted(set(SEAM_NAMES) - fix_keys)} extra={sorted(fix_keys - set(SEAM_NAMES))}")
            sys.exit(2)
        if fix_funcs != set(KINDS):
            print(f"FATAL oracle_telegram_adapter_flow10: fixture func set != KINDS: "
                  f"missing={sorted(set(KINDS) - fix_funcs)} extra={sorted(fix_funcs - set(KINDS))}")
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
    if shown >= 20:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:500]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:500]}")
    i, w, g = first_trace_diff(want, got)
    if i is not None:
        print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:500]}")
        print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:500]}")
    ws, gs = want.get("state") or {}, got.get("state") or {}
    if ws != gs:
        keys = [k for k in set(ws) | set(gs) if ws.get(k) != gs.get(k)]
        print(f"  state diff keys: {sorted(keys)[:10]}")
        for k in sorted(keys)[:4]:
            print(f"    {k}: want={json.dumps(ws.get(k), ensure_ascii=False)[:200]} got={json.dumps(gs.get(k), ensure_ascii=False)[:200]}")


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
    print(f"ORACLE telegram_adapter_flow10: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "telegram_adapter_flow10.jsonl"
    n = bad = 0
    counts = {}
    for line in fix.open(encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        case = json.loads(line)
        n += 1
        func = case["func"]
        counts[func] = counts.get(func, 0) + 1
        try:
            drive(func, case.get("in") or {})
        except BaseException as exc:
            bad += 1
            print(f"SELFTEST DRIVE RAISED {func} case {n}: {type(exc).__name__}: {exc}")
    if set(counts) != set(KINDS):
        print(f"SELFTEST KINDS DRIFT: fixture funcs={sorted(counts)} kinds={sorted(KINDS)}")
        bad += 1
    print(f"SELFTEST telegram_adapter_flow10: {n} cases, {bad} drive failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
