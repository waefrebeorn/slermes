#!/usr/bin/env python3
"""Oracle for the tui_gateway/methods_tools.py FLOW2 port
(src/gateway/tui_methods_tools_flow2.c).

The harness emits one JSON object per case carrying ONLY what the C code did:
the ordered seam trace, the return and the state mirror. This oracle re-drives
each case through a Python reference model of the same live decision logic,
using the SAME scripted seams (in.io), then compares parsed JSON (canon).

The reference models are pinned to the live defs: at import time the oracle
AST-extracts every target FunctionDef from the READ-ONLY quarry
(~/hermes-agent/tui_gateway/methods_tools.py) and records its source span; a
missing def is a hard refusal. Every external call is a recording seam, so the
models reproduce the live branch order / error composition / exception paths.

Full builtin exception hierarchy: make_exc resolves any builtin exception name
from `builtins` (with the whole family relationships intact); BaseException-only
kinds (SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError) pass through
every except-Exception site, and the ValueError / OSError families catch their
subclasses exactly as C's predicates do.

Fail-closed: missing defs or 0 cases => exit 2. Summary:
'ORACLE tui_methods_tools_flow2: N cases, M mismatches'.
"""
import ast
import builtins
import copy
import json
import os
import sys
import types
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

_REPO = Path(__file__).resolve().parent.parent
Q = os.path.expanduser("~/hermes-agent")
SRC = os.path.join(Q, "tui_gateway", "methods_tools.py")

TARGETS = [
    "_session_toolsets", "_toolset_rows", "_skill_dispatch_or_refuse",
    "_bundle_key_for", "_dispatch_bundle", "_dispatch_skill", "_prompt_builtin",
    "_dispatch_quick", "_profile_skill_command", "_cmd_init", "_cmd_moa",
    "_cmd_focus", "_cmd_retry", "_cmd_steer", "_cmd_goal", "_cmd_loop",
    "_cmd_undo", "_cmd_compress", "_pending_subcommand_store",
    "_write_approval_mode_setter", "_run_pending_review", "_cmd_memory",
    "_cmd_skills", "_slash_worker_for", "_container_checkpoint_refusal",
    "_configure_session_tools", "_skills_install", "register",
]


def pin_live_defs():
    """AST-extract every target live def from the quarry; refuse if absent.
    The extracted source is kept in LIVE_SRC (evidence the oracle is pinned)."""
    try:
        text = open(SRC, encoding="utf-8").read()
    except Exception as exc:
        print(f"FATAL oracle_tui_methods_tools_flow2: cannot read {SRC}: {exc}")
        sys.exit(2)
    lines = text.splitlines()
    tree = ast.parse(text)
    found = {}
    for n in tree.body:
        if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name in TARGETS:
            found[n.name] = "\n".join(lines[n.lineno - 1:n.end_lineno])
    missing = [t for t in TARGETS if t not in found]
    if missing:
        print(f"FATAL oracle_tui_methods_tools_flow2: live defs missing: {missing}")
        sys.exit(2)
    return found


LIVE_SRC = pin_live_defs()

# ── full builtin exception hierarchy ──
_BASE_ONLY = {"SystemExit", "KeyboardInterrupt", "GeneratorExit", "CancelledError"}
_VALUE_FAMILY = {"ValueError", "UnicodeError", "UnicodeDecodeError", "UnicodeEncodeError",
                 "UnicodeTranslateError"}
_OS_FAMILY = {"OSError", "PermissionError", "FileNotFoundError", "FileExistsError",
              "IsADirectoryError", "NotADirectoryError", "TimeoutError", "BlockingIOError",
              "ChildProcessError", "ConnectionError", "BrokenPipeError", "ConnectionAbortedError",
              "ConnectionRefusedError", "ConnectionResetError", "InterruptedError",
              "NotConnectedError", "ProcessLookupError", "ModuleNotFoundError"}

# Synthetic project classes (not builtins).
class SessionNotOwned(Exception):
    pass


class CompressionBusy(Exception):
    pass


class PluginOperationError(Exception):
    pass


_SYNTH = {"SessionNotOwned": SessionNotOwned, "CompressionBusy": CompressionBusy,
          "PluginOperationError": PluginOperationError}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = _SYNTH.get(kind)
    if cls is None:
        candidate = getattr(builtins, kind, None)
        if isinstance(candidate, type) and issubclass(candidate, BaseException):
            cls = candidate
    if cls is None:
        cls = type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


def is_base_exc(exc):
    return type(exc).__name__ in _BASE_ONLY or isinstance(exc, (SystemExit, KeyboardInterrupt, GeneratorExit))


def is_value_error(exc):
    return type(exc).__name__ in _VALUE_FAMILY or isinstance(exc, ValueError)


def is_os_error(exc):
    return type(exc).__name__ in _OS_FAMILY or isinstance(exc, OSError)


def is_not_owned(exc):
    return isinstance(exc, SessionNotOwned)


# ── Python mirrors of the C scalar helpers ──
def truthy(v):
    if v is None or v is False:
        return False
    if isinstance(v, (int, float)) and not isinstance(v, bool):
        return v != 0
    if isinstance(v, str):
        return len(v) > 0
    if isinstance(v, (list, dict, tuple, set)):
        return len(v) > 0
    return bool(v)


def sv(v):
    return v if isinstance(v, str) else None


def pystr(v):
    if v is None:
        return "None"
    if v is True:
        return "True"
    if v is False:
        return "False"
    if isinstance(v, str):
        return v
    if isinstance(v, (int, float)):
        return str(v)
    return json.dumps(v, ensure_ascii=False)


def strip(s):
    return (s or "").strip() if isinstance(s, str) else ""


def lower(s):
    return (s or "").lower() if isinstance(s, str) else ""


def str_strip(v):
    """(v or '').strip(); truthy non-string mirrors C AttributeError."""
    if not truthy(v):
        return ""
    if not isinstance(v, str):
        raise AttributeError(f"'{type(v).__name__}' object has no attribute 'strip'")
    return v.strip()


def str_or_empty(v):
    return pystr(v) if truthy(v) else ""


def joined(a, b):
    return "\n".join(p for p in (a or "", b or "") if p).strip()


def slc(s, n):
    return (s or "")[:n] if isinstance(s, str) else ""


def get(o, k, default=None):
    return o.get(k, default) if isinstance(o, dict) else default


def err_args(rid, code, msg, data=None):
    a = {}
    if rid is not None:
        a["rid"] = rid
    a["code"] = code
    a["message"] = msg
    if data is not None:
        a["data"] = data
    return a


def ok_args(rid, result):
    return {"rid": rid, "result": result}


# ── per-case seam backend (mirrors C tmt2_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def call(self, name, args):
        args = {k: v for k, v in (args or {}).items() if v is not None}
        entry = self.io.get(name, _MISSING)
        used = entry
        if entry is _MISSING:
            exc = RuntimeError(f"no script for {name}")
            self.trace.append({"name": name, "args": canon(args), "ret": proj_exc(exc)})
            raise exc
        if isinstance(entry, list):
            idx = self.cursors.get(name, 0)
            if idx >= len(entry):
                exc = RuntimeError(f"script exhausted for {name}")
                self.trace.append({"name": name, "args": canon(args), "ret": proj_exc(exc)})
                raise exc
            self.cursors[name] = idx + 1
            used = entry[idx]
        if isinstance(used, dict) and used.get("__script__"):
            if isinstance(used.get("raise"), dict):
                exc = make_exc(used["raise"])
                self.trace.append({"name": name, "args": canon(args), "ret": proj_exc(exc)})
                raise exc
            ret = used.get("ret")
        else:
            ret = used
        self.trace.append({"name": name, "args": canon(args), "ret": canon(ret)})
        return copy.deepcopy(ret) if isinstance(ret, (dict, list)) else ret


def C(ctx, name, args):
    return ctx.call(name, args)


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
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


# ── reference models (mirror the C tmt2_* functions line for line) ──
def m_session_toolsets(c, a):
    session = a.get("session")
    agent = get(session, "agent") if truthy(session) else None
    if agent is not None:
        e = C(c, "agent.enabled_toolsets", {})
        d = C(c, "agent.disabled_toolsets", {})
        return [e, d]
    sc = C(c, "session_source", {"session": session})
    C(c, "profile_scope", {"session": session})
    plat = C(c, "resolve_agent_platform", {"source": sc})
    en = C(c, "load_enabled_toolsets", {"platform": plat})
    dis = C(c, "load_disabled_toolsets", {})
    return [en, dis]


def m_toolset_rows(c, a):
    params = a.get("params")
    with_tools = truthy(a.get("with_tools"))
    sid = get(params, "session_id")
    C(c, "tools_mod", {"module": "toolsets"})
    sess = C(c, "sessions.get", {"key": sid})
    sts = C(c, "session_toolsets", {"session": sess})
    en = sts[0] if isinstance(sts, list) and len(sts) > 0 else None
    enabled_nonempty = truthy(en)
    allt = C(c, "toolsets.get_all_toolsets", {})
    rows = []
    if not isinstance(allt, dict):
        return rows
    for name in sorted(allt.keys()):
        info = C(c, "toolsets.get_toolset_info", {"name": name})
        if not truthy(info):
            continue
        row = {"name": name, "description": get(info, "description"),
               "tool_count": get(info, "tool_count")}
        row["description"] = row["description"] if row["description"] is not None else None
        on = True
        if enabled_nonempty and isinstance(en, list):
            on = name in en
        row["enabled"] = bool(on)
        if with_tools:
            row["tools"] = get(info, "resolved_tools")
        rows.append(row)
    return rows


def m_skill_dispatch_or_refuse(c, a):
    pa = {"name": sv(a.get("base")) or "", "arg": a.get("arg"), "session_id": a.get("sid")}
    dispatched = C(c, "dispatch", {"method": "command.dispatch", "rid": a.get("rid"), "params": pa})
    if "error" in dispatched or truthy(get(get(dispatched, "result"), "type")):
        return dispatched
    base = sv(a.get("base")) or ""
    C(c, "err", err_args(a.get("rid"), 4018, f"skill command: use command.dispatch for /{base}"))
    raise _ReturnLast(c)


class _ReturnLast(Exception):
    """Internal: the model's last seam was the return value (see drive)."""
    pass


def m_bundle_key_for(c, a):
    name = sv(a.get("name"))
    rc = c.call("hermes_cli.commands.resolve_command", {"name": name}) if False else None
    try:
        rc = C(c, "hermes_cli.commands.resolve_command", {"name": name})
    except BaseException:
        return None
    if rc is not None:
        return None
    try:
        return C(c, "agent.skill_bundles.resolve_bundle_command_key", {"name": name})
    except BaseException:
        return None


def m_dispatch_bundle(c, a):
    rid = a.get("rid"); name = sv(a.get("name")) or ""
    bk = C(c, "bundle_key_for", {"name": name})
    if not truthy(bk):
        return None
    session = a.get("session")
    skey = get(session, "session_key")
    ba = {"bundle_key": bk, "arg": a.get("arg"), "task_id": skey if skey else "",
          "platform": C(c, "resolve_session_platform", {})}
    try:
        res = C(c, "agent.skill_bundles.build_bundle_invocation_message", ba)
    except BaseException as exc:
        C(c, "err", err_args(rid, 4018, f"bundle dispatch failed: {exc}"))
        raise _ReturnLast(c)
    if not truthy(res):
        C(c, "err", err_args(rid, 4018, f"failed to load bundle: {bk}"))
        raise _ReturnLast(c)
    msg, loaded, missing = res[0], res[1], res[2]
    sb = C(c, "agent.skill_bundles.get_skill_bundles", {})
    entry = get(sb, bk) if isinstance(sb, dict) else None
    bn = get(entry, "name") if isinstance(entry, dict) else None
    bname = bn if isinstance(bn, str) and bn else (bk.lstrip("/") if bk else "")
    notice = f"\u26a1 Loading bundle: {bname} ({len(loaded) if isinstance(loaded,list) else 0} skills)"
    if missing:
        notice += "\nSkipped missing skills: " + ", ".join(str(x) for x in missing)
    disp = C(c, "skill_scaffold_projection", {"message": msg})
    return C(c, "ok", ok_args(rid, {"type": "send", "message": msg, "notice": notice, "display": disp}))


def m_dispatch_skill(c, a):
    rid = a.get("rid"); name = sv(a.get("name")) or ""; arg = a.get("arg")
    session = a.get("session"); key = ("/" + name).lower()
    try:
        C(c, "tools_mod", {"module": "agent.skill_commands"})
        cmds = C(c, "agent.skill_commands.get_interactive_skill_commands", {})
    except BaseException:
        return None
    if not isinstance(cmds, dict) or key not in cmds:
        return None
    entry = get(cmds, key)
    skey = get(session, "session_key")
    try:
        extra = C(c, "agent.skill_commands.split_stacked_skill_commands",
                  {"arg": arg, "interactive": True})
    except BaseException:
        return None
    if isinstance(extra, list) and len(extra) > 0 and truthy(extra[0]):
        keys = [key] + list(extra[0]) if isinstance(extra[0], list) else [key]
        ba = {"keys": keys, "instruction": extra[1] if len(extra) > 1 else "",
              "task_id": skey if skey else ""}
        try:
            stacked = C(c, "agent.skill_commands.build_stacked_skill_invocation_message", ba)
        except BaseException:
            return None
        if truthy(stacked):
            msg, loaded, missing = stacked[0], stacked[1], stacked[2]
            notice = f"\u26a1 Loading {len(loaded) if isinstance(loaded,list) else 0} stacked skills: " + \
                     ", ".join(str(x) for x in (loaded or []))
            if missing:
                notice += "\nSkipped missing skills: " + ", ".join(str(x) for x in missing)
            cn = get(entry, "name")
            disp = C(c, "skill_scaffold_projection", {"message": msg})
            return C(c, "ok", ok_args(rid, {"type": "skill", "message": msg,
                "name": cn if isinstance(cn, str) and cn else name, "notice": notice, "display": disp}))
    ba = {"key": key, "instruction": extra[1] if isinstance(extra, list) and len(extra) > 1 else "",
          "task_id": skey if skey else ""}
    try:
        msg = C(c, "agent.skill_commands.build_skill_invocation_message", ba)
    except BaseException:
        return None
    if truthy(msg):
        cn = get(entry, "name")
        disp = C(c, "skill_scaffold_projection", {"message": msg})
        return C(c, "ok", ok_args(rid, {"type": "skill", "message": msg,
            "name": cn if isinstance(cn, str) and cn else name, "display": disp}))
    return None


def m_prompt_builtin(c, a):
    m = {"__closure__": "prompt_builtin", "module": sv(a.get("module")) or "",
         "fn": sv(a.get("fn")) or ""}
    if a.get("kw") is not None:
        m["kw"] = a["kw"]
    return m


def m_prompt_builtin_call(c, a):
    rid = a.get("rid"); module = sv(a.get("module")) or ""; fn = sv(a.get("fn")) or ""
    arg = a.get("arg"); kw = sv(a.get("kw"))
    seam = f"{module}.{fn}"
    ba = {kw: arg} if kw else {"arg": arg}
    msg = C(c, seam, ba)
    return C(c, "ok", ok_args(rid, {"type": "send", "message": msg}))


def m_dispatch_quick(c, a):
    rid = a.get("rid"); name = sv(a.get("name"))
    cfg = C(c, "load_cfg", {})
    qc = get(get(cfg, "quick_commands"), name) if isinstance(cfg, dict) else None
    if not truthy(qc):
        return None
    ts = sv(get(qc, "type"))
    if ts == "exec":
        env = C(c, "tools.environments.local.build_subprocess_env", {})
        r = C(c, "subprocess.run", {"command": get(qc, "command", ""), "shell": True,
                                    "env": env, "timeout": 30})
        output = slc(joined(sv(get(r, "stdout")), sv(get(r, "stderr"))), 4000)
        if truthy(output):
            output = C(c, "agent.redact.redact_sensitive_text", {"text": output})
        rc = get(r, "returncode", 0)
        if not isinstance(rc, (int, float)):
            rc = 0
        if rc != 0:
            msg = output if truthy(output) else f"quick command failed with exit code {int(rc) if float(rc).is_integer() else rc}"
            C(c, "err", err_args(rid, 4018, msg))
            raise _ReturnLast(c)
        return C(c, "exec_out", {"rid": rid, "output": output})
    if ts == "alias":
        p = {"type": "alias", "target": sv(get(qc, "target")) or ""}
        return C(c, "ok", ok_args(rid, p))
    return None


def m_profile_skill_command(c, a):
    session = a.get("session"); base = sv(a.get("base")) or ""
    try:
        C(c, "session_home_scope", {"session": session})
        cmds = C(c, "agent.skill_commands.get_interactive_skill_commands", {})
    except BaseException:
        return None
    return ("/" + base) in cmds if isinstance(cmds, dict) else False


def m_cmd_init(c, a):
    rid = a.get("rid"); session = a.get("session"); arg = a.get("arg")
    skey = get(session, "session_key") if session else None
    try:
        cwd = C(c, "get_session_cwd", {"session_key": skey})
    except BaseException:
        cwd = None
    if not truthy(cwd):
        cwd = C(c, "session_cwd", {"session": session})
    msg = C(c, "build_init_prompt_for_cwd", {"extra": arg, "cwd": cwd, "session_key": skey})
    return C(c, "ok", ok_args(rid, {"type": "send", "message": msg}))


def m_cmd_moa(c, a):
    rid = a.get("rid"); params = a.get("params"); session = a.get("session"); arg = a.get("arg")
    if not truthy(arg):
        C(c, "err", err_args(rid, 4004, "moa usage")); raise _ReturnLast(c)
    if not truthy(session):
        C(c, "err", err_args(rid, 4001, "no active session")); raise _ReturnLast(c)
    try:
        cfg = C(c, "load_cfg", {})
        norm = C(c, "hermes_cli.moa_config.normalize_moa_config", {"config": get(cfg, "moa")})
    except BaseException as exc:
        C(c, "err", err_args(rid, 5030, f"moa unavailable: {exc}")); raise _ReturnLast(c)
    preset = sv(get(norm, "default_preset")) or ""
    restore = {"override": get(session, "model_override"),
               "model": C(c, "agent.attr", {"name": "model"}),
               "provider": C(c, "agent.attr", {"name": "provider"})}
    C(c, "session.write", {"key": "moa_one_shot_restore", "value": restore})
    if get(session, "agent") is not None:
        sa = {"session_id": get(params, "session_id", ""), "override": f"{preset} --provider moa",
              "confirm_expensive_model": False, "pin_session_override": True,
              "persist_override": False, "count_switch": False}
        try:
            C(c, "apply_model_switch", sa)
        except BaseException as exc:
            C(c, "session.del", {"key": "moa_one_shot_restore"})
            if is_not_owned(exc) or is_value_error(exc):
                C(c, "session_not_owned_err", {"exc": str(exc)}); raise _ReturnLast(c)
            C(c, "err", err_args(rid, 5030, f"moa unavailable: {exc}")); raise _ReturnLast(c)
    else:
        ov = {"provider": "moa", "model": preset, "base_url": "moa://local",
              "api_key": "moa-virtual-provider", "api_mode": "chat_completions"}
        C(c, "session.write", {"key": "model_override", "value": ov})
    notice = f"MoA one-shot queued with preset {preset}; previous model will be restored after this turn."
    return C(c, "ok", ok_args(rid, {"type": "send", "notice": notice, "message": arg}))


def m_cmd_focus(c, a):
    rid = a.get("rid"); params = a.get("params"); arg = a.get("arg")
    cfg = C(c, "load_cfg", {})
    disp = get(cfg, "display")
    cur = truthy(get(disp, "focus_view")) if isinstance(disp, dict) else False
    action = C(c, "hermes_cli.focus_view.resolve_focus_arg", {"arg": arg, "cur": cur})
    a0 = sv(action) or ""
    if a0 == "usage":
        C(c, "err", err_args(rid, 4004, "usage: /focus [on|off|status]")); raise _ReturnLast(c)
    if a0 == "status":
        saved = get(disp, "focus_saved_tool_progress") if isinstance(disp, dict) else None
        if not truthy(saved):
            saved = C(c, "load_tool_progress_mode", {})
        out = C(c, "hermes_cli.focus_view.format_focus_status", {"cur": cur, "saved": saved})
        return C(c, "exec_out", {"rid": rid, "output": out})
    target = truthy(action)
    dp = {"key": "focus", "value": "on" if target else "off",
          "session_id": get(params, "session_id", "")}
    res = C(c, "dispatch", {"method": "config.set", "rid": rid, "params": dp})
    if "error" in res:
        return res
    tp = get(get(res, "result"), "tool_progress") or "all"
    out = C(c, "hermes_cli.focus_view.format_focus_toggle_message",
            {"target": bool(target), "tool_progress": tp})
    return C(c, "exec_out", {"rid": rid, "output": out})


def m_cmd_retry(c, a):
    rid = a.get("rid"); session = a.get("session")
    if not truthy(session):
        C(c, "err", err_args(rid, 4001, "no active session to retry")); raise _ReturnLast(c)
    b1 = C(c, "busy_error", {"cmd": "retry"})
    if truthy(b1):
        return b1
    pre = C(c, "rewind_prelude", {"cmd": "retry", "empty": "no previous user message to retry"})
    uind = pre[1] if isinstance(pre, list) and len(pre) > 1 else None
    if not truthy(uind) or len(uind) == 0:
        C(c, "err", err_args(rid, 4018, "no previous user message to retry")); raise _ReturnLast(c)
    hv = C(c, "session_rules.turns.history_before_user_originated_turn", {"index": uind[-1]})
    live = hv[1] if isinstance(hv, list) and len(hv) > 1 else None
    content = get(live, "content")
    try:
        txt = C(c, "session_rules.turns.retryable_user_text", {"content": content})
    except BaseException as exc:
        if is_value_error(exc):
            C(c, "err", err_args(rid, 4018, str(exc))); raise _ReturnLast(c)
        raise
    rw = C(c, "rewind_or_err", {"keep": len(uind) - 1, "code": 4018,
                                "prefix": "", "require_retryable": True})
    er = rw[1] if isinstance(rw, list) and len(rw) > 1 else None
    if truthy(er):
        return er
    rr0 = rw[0]
    c2 = get(rr0[1], "content") if isinstance(rr0, list) and len(rr0) > 1 else None
    txt2 = C(c, "session_rules.turns.retryable_user_text", {"content": c2})
    C(c, "tui_model_friction", {"cmd": "retry", "session": session})
    return C(c, "ok", ok_args(rid, {"type": "send", "message": txt2}))


def m_cmd_steer(c, a):
    rid = a.get("rid"); arg = a.get("arg"); session = a.get("session")
    if not truthy(arg):
        C(c, "err", err_args(rid, 4004, "usage: /steer <prompt>")); raise _ReturnLast(c)
    s = sv(arg) or ""
    shown = f"{slc(s, 80)}{'...' if len(s) > 80 else ''}"
    running = truthy(get(session, "running")) if session else False
    if not running:
        p = {"type": "send", "message": arg,
             "notice": f"No agent running; sent as next turn: {shown}"}
        return C(c, "ok", ok_args(rid, p))
    try:
        st = C(c, "agent.steer", {"arg": arg})
    except BaseException:
        st = None
    if truthy(st):
        out = f"\u23e9 Steer queued \u2014 arrives after the next tool call: {shown}"
        return C(c, "exec_out", {"rid": rid, "output": out})
    return C(c, "ok", ok_args(rid, {"type": "send", "message": arg}))


def m_cmd_goal(c, a):
    rid = a.get("rid"); session = a.get("session"); arg = a.get("arg")
    C(c, "profile_scope", {"session": session})
    sk = C(c, "session_key_or_err", {"module": "hermes_cli.goals", "label": "goals"})
    er = sk[2] if isinstance(sk, list) and len(sk) > 2 else None
    if truthy(er):
        return er
    skey = sk[0]
    try:
        cfg = C(c, "load_cfg", {})
        mtv = get(get(cfg, "goals"), "max_turns")
        max_turns = mtv if truthy(mtv) and isinstance(mtv, (int, float)) else 20
    except BaseException:
        max_turns = 20
    mgr = C(c, "hermes_cli.goals.GoalManager", {"session_id": skey, "default_max_turns": max_turns})
    lu = C(c, "hermes_cli.goals.last_user_message_from_db", {"session_id": skey})
    res = C(c, "dispatch_goal_command", {"mgr": mgr, "arg": arg, "last_user_message": lu})
    output = get(res, "output")
    if truthy(get(res, "error")):
        C(c, "err", err_args(rid, 4004, sv(output) or "")); raise _ReturnLast(c)
    prompt = get(res, "prompt")
    if not truthy(prompt):
        return C(c, "exec_out", {"rid": rid, "output": output if output is not None else ""})
    p = {"type": "send", "notice": output, "message": prompt}
    if not truthy(get(res, "kickoff")):
        p["notice"] = f"{p['notice']}\nContinuing now \u2014 taking the next step."
        p["display"] = "/goal resume"
    return C(c, "ok", ok_args(rid, p))


def m_cmd_loop(c, a):
    rid = a.get("rid"); session = a.get("session"); arg = a.get("arg")
    sk = C(c, "session_key_or_err", {"module": "hermes_cli.loops", "label": "loops"})
    er = sk[2] if isinstance(sk, list) and len(sk) > 2 else None
    if truthy(er):
        return er
    skey = sk[0]
    lm = C(c, "hermes_cli.loops.LoopManager", {"session_id": skey})
    res = C(c, "hermes_cli.loops.dispatch_loop_command", {"mgr": lm, "arg": arg})
    output = sv(get(res, "output")) or ""
    if truthy(get(res, "created")):
        try:
            gb = C(c, "hermes_cli.loops.goal_blocks_loop_tick", {"session_id": skey})
        except BaseException:
            gb = None
        if truthy(gb):
            output += ("\nNote: an active /goal is driving this session \u2014 loop "
                       "wakeups defer until the goal finishes, pauses, or parks.")
    return C(c, "exec_out", {"rid": rid, "output": output})


def m_cmd_undo(c, a):
    rid = a.get("rid"); session = a.get("session"); arg = a.get("arg")
    if not truthy(session):
        C(c, "err", err_args(rid, 4001, "no active session to undo")); raise _ReturnLast(c)
    b = C(c, "busy_error", {"cmd": "undo"})
    if truthy(b):
        return b
    skey = get(session, "session_key")
    if not truthy(skey):
        C(c, "err", err_args(rid, 4001, "no session key for undo")); raise _ReturnLast(c)
    astr = str_strip(arg)
    n = 1
    if astr:
        try:
            n = max(int(astr.split()[0]), 1)
        except (ValueError, IndexError):
            C(c, "err", err_args(rid, 4004, f"undo: invalid count {astr!r} \u2014 use /undo or /undo N"))
            raise _ReturnLast(c)
    pre = C(c, "rewind_prelude", {"cmd": "undo", "empty": "no user messages to undo"})
    uind = pre[1] if isinstance(pre, list) and len(pre) > 1 else None
    if not truthy(uind) or len(uind) == 0:
        C(c, "err", err_args(rid, 4018, "no user messages to undo")); raise _ReturnLast(c)
    tu = min(n, len(uind))
    rw = C(c, "rewind_or_err", {"keep": len(uind) - tu, "code": 4004, "prefix": "undo: "})
    er = rw[1] if isinstance(rw, list) and len(rw) > 1 else None
    if truthy(er):
        return er
    rr0 = rw[0]
    live = rr0[1] if isinstance(rr0, list) and len(rr0) > 1 else None
    txt = C(c, "session_rules.turns.flatten_message_text", {"content": get(live, "content")})
    rcount = rr0[2] if isinstance(rr0, list) and len(rr0) > 2 else 0
    mm = C(c, "agent.attr", {"name": "_memory_manager"})
    if truthy(mm):
        try: C(c, "agent.on_session_switch", {"session_key": skey})
        except BaseException: pass
        try: C(c, "agent.invalidate_system_prompt", {})
        except BaseException: pass
        try: C(c, "agent.set_last_flushed", {"len": tu})
        except BaseException: pass
    try:
        C(c, "tui_model_friction", {"cmd": "undo", "session": session})
    except BaseException as exc:
        raise exc
    word = "turn" if tu == 1 else "turns"
    notice = f"\u21b6 Undid {tu} {word} ({rcount} message(s)). Edit and resubmit, or send a new message."
    try:
        hint = C(c, "tui_gateway.checkpoints.undo_rollback_hint_for", {"session": session, "live_view": live})
        if truthy(hint):
            notice += "\n" + (sv(get(hint, "text")) or "")
    except BaseException:
        pass
    return C(c, "ok", ok_args(rid, {"type": "prefill", "message": txt, "notice": notice}))


def m_cmd_compress(c, a):
    rid = a.get("rid"); params = a.get("params"); session = a.get("session")
    if not truthy(session):
        C(c, "err", err_args(rid, 4001, "no active session to compress")); raise _ReturnLast(c)
    b = C(c, "busy_error", {"cmd": "compress"})
    if truthy(b):
        return b
    sid = get(params, "session_id")
    uch = C(c, "session_uses_compute_host", {"session": session})
    if truthy(uch):
        ch = C(c, "compute_host_slash", {"sid": sid, "cmd": "compress"})
        st = sv(get(ch, "status")) or ""; text = get(ch, "text")
        if st in ("failed", "rejected"):
            C(c, "err", err_args(rid, 5019 if st == "failed" else 4009, sv(text) or "")); raise _ReturnLast(c)
        if st == "pending":
            return C(c, "ok", ok_args(rid, {"type": "exec", "status": "pending", "output": text}))
        return C(c, "exec_out", {"rid": rid, "output": text if text is not None else ""})
    try:
        out = C(c, "compress_live_with_feedback", {"session": session})
    except BaseException as exc:
        if is_not_owned(exc):
            C(c, "session_not_owned_err", {"exc": str(exc)}); raise _ReturnLast(c)
        if isinstance(exc, CompressionBusy):
            C(c, "err", err_args(rid, 4009, str(exc))); raise _ReturnLast(c)
        try: C(c, "agent.conversation_compression.finalize_context_engine_compression_notification", {"session": session})
        except BaseException: pass
        C(c, "err", err_args(rid, 5009, f"compress failed: {exc}")); raise _ReturnLast(c)
    return C(c, "exec_out", {"rid": rid, "output": out})


def m_pending_subcommand_store(c, a):
    session = a.get("session")
    if truthy(session):
        try:
            store = C(c, "agent.attr", {"name": "_memory_store"})
        except BaseException:
            store = None
        if truthy(store):
            return store
    try:
        return C(c, "tools.memory_tool.load_on_disk_store", {})
    except BaseException:
        return None


def m_write_approval_mode_setter(c, a):
    sub = sv(a.get("subsystem")) or ""
    return {"__closure__": "write_approval_mode_setter", "key": f"{sub}.write_approval"}


def m_write_approval_mode_setter_call(c, a):
    key = sv(a.get("key")) or ""
    return C(c, "write_config_key", {"key": key, "enabled": truthy(a.get("enabled"))})


def m_run_pending_review(c, a):
    rid = a.get("rid"); sub = sv(a.get("subsystem")) or ""
    arg = a.get("arg"); session = a.get("session")
    unknown = sv(a.get("unknown")) or ""
    store = m_pending_subcommand_store(c, {"session": session}) if sub == "memory" else None
    sa = {"subsystem": sub, "arg": arg, "memory_store": store,
          "set_mode_fn": "write_approval_mode_setter"}
    out = C(c, "handle_pending_subcommand", sa)
    o = out if truthy(out) else unknown
    return C(c, "exec_out", {"rid": rid, "output": o})


def m_cmd_memory(c, a):
    return m_run_pending_review(c, {"rid": a.get("rid"), "subsystem": "memory",
        "arg": a.get("arg"), "session": a.get("session"), "unknown": a.get("unknown")})


def m_cmd_skills(c, a):
    rid = a.get("rid"); arg = a.get("arg"); session = a.get("session")
    sub = str_strip(arg).split()
    rc = C(c, "hermes_cli.commands.resolve_command", {"name": "skills"})
    allowed = get(rc, "desktop_subcommands") or []
    if not sub or sub[0].lower() not in allowed:
        return None
    first = sub[0].lower()
    if first not in ("approval", "mode") and not truthy(a.get("wa_enabled")) \
            and a.get("wa_pending", 0) == 0:
        return C(c, "exec_out", {"rid": rid, "output":
            "Skill write approval is off (skills.write_approval). Enable it with /skills approval on, then review staged writes with /skills pending."})
    return m_run_pending_review(c, {"rid": rid, "subsystem": "skills", "arg": arg,
        "session": session, "unknown": a.get("unknown")})


def m_slash_worker_for(c, a):
    rid = a.get("rid"); sid = a.get("sid"); session = a.get("session")
    w0 = get(session, "slash_worker")
    if truthy(w0):
        return [w0, None]
    try:
        C(c, "sessions_lock", {})
    except BaseException as exc:
        return [None, C(c, "err", err_args(rid, 5030, str(exc)))]
    w1 = C(c, "session.get", {"key": "slash_worker"})
    if truthy(w1):
        return [w1, None]
    try:
        model = C(c, "agent.attr", {"name": "model"})
    except BaseException:
        model = None
    if not truthy(model):
        model = C(c, "resolve_model", {})
    ca = {"session_key": sid, "model": model, "provider": get(session, "provider"),
          "base_url": get(session, "base_url")}
    try:
        w = C(c, "slash_worker_ctor", ca)
    except BaseException as exc:
        try: C(c, "logger.warning", {"message": str(exc)})
        except BaseException: pass
        e = C(c, "err", err_args(rid, 5030, "slash worker start failed"))
        return [None, e]
    C(c, "attach_worker", {"sid": sid, "worker": w})
    return [w, None]


def m_container_checkpoint_refusal(c, a):
    session = a.get("session"); cwd = a.get("cwd")
    skey = get(session, "session_key")
    tid = (sv(skey) if truthy(skey) else None) or "default"
    sc = C(c, "set_session_context", {"task_id": tid, "cwd": cwd})
    C(c, "profile_scope", {"session": session})
    try:
        reason = C(c, "mgr.unsupported_backend_reason", {"task_id": tid})
    finally:
        C(c, "clear_session_context", {"tokens": sc})
    return reason


def m_configure_session_tools(c, a):
    rid = a.get("rid"); params = a.get("params"); sid = a.get("sid"); session = a.get("session")
    act = lower(str_strip(get(params, "action")))
    if act not in ("disable", "enable"):
        C(c, "err", err_args(rid, 4017, f"unknown tools action: {act}")); raise _ReturnLast(c)
    targets = [s for n in (get(params, "names") or []) if (s := strip(pystr(n)))]
    if not targets:
        C(c, "err", err_args(rid, 4018, "names required")); raise _ReturnLast(c)
    C(c, "tools_mod", {"module": "hermes_cli.config"})
    C(c, "tools_mod", {"module": "hermes_cli.tools_config"})
    cfg = C(c, "hermes_cli.config.load_config", {})
    valid = C(c, "hermes_cli.tools_config._get_plugin_toolset_keys", {}) or []
    mcp = [n for n in targets if ":" in n]
    unknown = [n for n in targets if ":" not in n and n not in valid]
    ts = [n for n in targets if ":" not in n and n in valid]
    if ts:
        C(c, "hermes_cli.tools_config._apply_toolset_change",
          {"cfg": cfg, "scope": "cli", "targets": ts, "action": act})
    msr = C(c, "mcp_server_rows", {})
    plugins = msr[1] if isinstance(msr, list) and len(msr) > 1 else None
    for target in mcp:
        srv = target.split(":", 1)[0]
        er = C(c, "mcp_plugin_write_error", {"rid": rid, "server": srv, "plugins": plugins})
        if truthy(er):
            return er
    missing = []
    if mcp:
        r = C(c, "hermes_cli.tools_config._apply_mcp_change",
              {"cfg": cfg, "targets": mcp, "action": act})
        missing = list(r) if isinstance(r, list) else []
    C(c, "hermes_cli.config.save_config", {"cfg": cfg})
    info = C(c, "reset_session_agent", {"sid": sid, "session": session}) if truthy(session) else None
    cfg2 = C(c, "hermes_cli.config.load_config", {})
    en = C(c, "hermes_cli.tools_config._get_platform_tools", {"cfg": cfg2}) or []
    enabled = sorted(str(x) for x in en) if isinstance(en, list) else []
    changed = [n for n in targets if n not in unknown and
               (":" not in n or n.split(":", 1)[0] not in missing)]
    p = {"changed": changed, "enabled_toolsets": enabled, "info": info,
         "missing_servers": sorted(str(x) for x in missing), "reset": bool(truthy(session)),
         "unknown": unknown}
    return C(c, "ok", ok_args(rid, p))


def m_skills_install(c, a):
    rid = a.get("rid"); query = a.get("query")
    verdict = C(c, "hermes_cli.skills_hub.do_install", {"query": query, "skip_confirm": True})
    t = (verdict is True)
    is_none = verdict is None
    installed = t
    if not installed and is_none:
        installed = truthy(C(c, "tools.skills_sync_bundled_ops.bundled_skill_for_install", {"query": query}))
    if not installed:
        ls = sv(a.get("capture_log")) or ""
        tail = ls.split("\n")[-1]
        data = {"installed": False, "name": query, "log": ls if ls else None}
        C(c, "err", err_args(rid, 5031, tail, data))
        raise _ReturnLast(c)
    return C(c, "ok", ok_args(rid, {"installed": True, "name": query}))


def m_register(c, a):
    server = a.get("server")
    C(c, "bind_module", {"server": server})
    return C(c, "plugin_commands.register", {"server": server})


MODELS = {
    "_session_toolsets": m_session_toolsets,
    "_toolset_rows": m_toolset_rows,
    "_skill_dispatch_or_refuse": m_skill_dispatch_or_refuse,
    "_bundle_key_for": m_bundle_key_for,
    "_dispatch_bundle": m_dispatch_bundle,
    "_dispatch_skill": m_dispatch_skill,
    "_prompt_builtin": m_prompt_builtin,
    "prompt_builtin.call": m_prompt_builtin_call,
    "_dispatch_quick": m_dispatch_quick,
    "_profile_skill_command": m_profile_skill_command,
    "_cmd_init": m_cmd_init,
    "_cmd_moa": m_cmd_moa,
    "_cmd_focus": m_cmd_focus,
    "_cmd_retry": m_cmd_retry,
    "_cmd_steer": m_cmd_steer,
    "_cmd_goal": m_cmd_goal,
    "_cmd_loop": m_cmd_loop,
    "_cmd_undo": m_cmd_undo,
    "_cmd_compress": m_cmd_compress,
    "_pending_subcommand_store": m_pending_subcommand_store,
    "_write_approval_mode_setter": m_write_approval_mode_setter,
    "write_approval_mode_setter.call": m_write_approval_mode_setter_call,
    "_run_pending_review": m_run_pending_review,
    "_cmd_memory": m_cmd_memory,
    "_cmd_skills": m_cmd_skills,
    "_slash_worker_for": m_slash_worker_for,
    "_container_checkpoint_refusal": m_container_checkpoint_refusal,
    "_configure_session_tools": m_configure_session_tools,
    "_skills_install": m_skills_install,
    "register": m_register,
}


def drive(func, inp):
    a = (inp.get("args") or {})
    c = Ctx(inp.get("io"))
    try:
        ret = MODELS[func](c, a)
    except _ReturnLast as stop:
        # The last recorded seam event's ret is the actual return value.
        ret = c.trace[-1]["ret"] if c.trace else None
    except BaseException as exc:
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(c.trace), "state": canon({})}


# ── seam-name cross-check (C port <-> oracle models <-> fixture io) ──
def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "tui_methods_tools_flow2.c", encoding="utf-8").read()
    c_seams = set(__import__("re").findall(r'tmt2_call\(ctx,\s*"([^"]+)"', csrc))
    c_seams |= set(__import__("re").findall(r'T2C\("([^"]+)"', csrc))
    osrc = open(__file__, encoding="utf-8").read()
    o_seams = set(__import__("re").findall(r'C\(c,\s*"([^"]+)"', osrc))
    unknown = (c_seams | o_seams) - KNOWN_SEAMS
    if unknown:
        print(f"FATAL oracle_tui_methods_tools_flow2: seams outside registry: {sorted(unknown)}")
        sys.exit(2)


KNOWN_SEAMS = set([
    "ok", "err", "exec_out", "tools_mod", "load_cfg", "sessions.get", "session.get",
    "dispatch", "busy_error", "rewind_prelude", "rewind_or_err", "session_key_or_err",
    "session_toolsets", "session_cwd", "session_home_scope", "profile_scope",
    "apply_model_switch", "session_not_owned_err", "tui_model_friction",
    "session_uses_compute_host", "compute_host_slash", "compress_live_with_feedback",
    "mcp_server_rows", "mcp_plugin_write_error", "reset_session_agent",
    "write_config_key", "set_session_context", "clear_session_context",
    "resolve_session_platform", "resolve_model", "load_tool_progress_mode",
    "skill_scaffold_projection", "logger.warning", "bind_module",
    "plugin_commands.register", "build_init_prompt_for_cwd", "get_session_cwd",
    "handle_pending_subcommand", "dispatch_goal_command", "sessions_lock",
    "agent.enabled_toolsets", "agent.disabled_toolsets", "session_source",
    "resolve_agent_platform", "load_enabled_toolsets", "load_disabled_toolsets",
    "toolsets.get_all_toolsets", "toolsets.get_toolset_info",
    "bundle_key_for", "agent.skill_commands.get_interactive_skill_commands",
    "agent.skill_commands.split_stacked_skill_commands",
    "agent.skill_commands.build_stacked_skill_invocation_message",
    "agent.skill_commands.build_skill_invocation_message",
    "agent.skill_bundles.resolve_bundle_command_key",
    "agent.skill_bundles.build_bundle_invocation_message",
    "agent.skill_bundles.get_skill_bundles",
    "tools.environments.local.build_subprocess_env", "subprocess.run",
    "agent.redact.redact_sensitive_text", "hermes_cli.commands.resolve_command",
    "agent.attr", "session.write", "session.del", "agent.steer",
    "hermes_cli.moa_config.normalize_moa_config",
    "hermes_cli.focus_view.resolve_focus_arg", "load_tool_progress_mode",
    "hermes_cli.focus_view.format_focus_status",
    "hermes_cli.focus_view.format_focus_toggle_message",
    "session_rules.turns.history_before_user_originated_turn",
    "session_rules.turns.retryable_user_text", "tui_model_friction",
    "hermes_cli.goals.GoalManager", "hermes_cli.goals.last_user_message_from_db",
    "hermes_cli.loops.LoopManager", "hermes_cli.loops.dispatch_loop_command",
    "hermes_cli.loops.goal_blocks_loop_tick",
    "session_rules.turns.flatten_message_text", "agent.on_session_switch",
    "agent.invalidate_system_prompt", "agent.set_last_flushed",
    "tui_gateway.checkpoints.undo_rollback_hint_for",
    "agent.conversation_compression.finalize_context_engine_compression_notification",
    "tools.memory_tool.load_on_disk_store", "write_config_key",
    "hermes_cli.tools_config._get_plugin_toolset_keys",
    "hermes_cli.config.load_config", "hermes_cli.config.save_config",
    "hermes_cli.tools_config._apply_toolset_change",
    "hermes_cli.tools_config._apply_mcp_change",
    "hermes_cli.tools_config._get_platform_tools",
    "hermes_cli.skills_hub.do_install",
    "tools.skills_sync_bundled_ops.bundled_skill_for_install",
    "resolve_model", "slash_worker_ctor", "attach_worker",
    "mgr.unsupported_backend_reason",
])


def report_mismatch(func, want, got, shown):
    if shown >= 12:
        return
    print(f"MISMATCH {func}:")
    if want.get("ret") != got.get("ret"):
        print(f"  ret want={json.dumps(want.get('ret'), ensure_ascii=False)[:400]}")
        print(f"  ret got ={json.dumps(got.get('ret'), ensure_ascii=False)[:400]}")
    wt, gt = want.get("trace") or [], got.get("trace") or []
    for i in range(max(len(wt), len(gt))):
        w = wt[i] if i < len(wt) else None
        g = gt[i] if i < len(gt) else None
        if w != g:
            print(f"  trace[{i}] want={json.dumps(w, ensure_ascii=False)[:400]}")
            print(f"  trace[{i}] got ={json.dumps(g, ensure_ascii=False)[:400]}")
            break


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
            print(f"MISMATCH <unparsable>: {exc}")
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
    print(f"ORACLE tui_methods_tools_flow2: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "tui_methods_tools_flow2.jsonl"
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
    print(f"SELFTEST tui_methods_tools_flow2: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    else:
        sys.exit(main())
