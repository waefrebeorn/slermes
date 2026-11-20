#!/usr/bin/env python3
"""Oracle for the TUI slash-exec misc method FLOW port
(src/gateway/tui_methods_misc_flow.c).

MODULE RESOLUTION: the task named tui_gateway/methods_misc.py, which does
NOT exist in the quarry. The miscellaneous slash.exec helpers (live command
output + side-effect mirroring) are defined in tui_gateway/methods_slash.py
— fully UNCLAIMED at port time. That is the REAL defining module the port
credits. The bodies are rebound onto tui_gateway/server.py's globals at
install time (method_ctx.bind_module), so every server global enters through
a recording seam.

Drives the LIVE methods_slash bodies with SCRIPTED seams: every in-function
import, server global, CM, agent method and ported sibling is a recording
stub fed the same scripts the C harness feeds its universal seam. Compares
the DECISION SEQUENCE (ordered seam events {"name","args","ret"} in "trace"),
the final return ("ret") and the mutated state mirror ("state":
agent.service_tier, materialized by _mirror_fast).

Live defs are AST-extracted from tui_gateway/methods_slash.py (module
Assign constants seeded, FunctionDefs exec'd with the __future__ annotations
flag into a registered sys.modules module — py3.14); the _LIVE_SLASH_OUTPUT
/ _SLASH_MIRRORS tables are re-exec'd per case AFTER the sibling recorders
are installed so their direct function references capture the seams. The
heavy quarry import chain is never imported. Ported siblings are SEAMED
inside flows (each is exercised by its own direct cases, where the real
body runs).

Fail-closed: missing defs or 0 cases => exit 2; a missing/exhausted seam
script is a recorded RuntimeError raise on BOTH sides. Per-case wrapper
catches BaseException (SystemExit included). Summary:
'ORACLE tui_methods_misc_flow: N cases, M mismatches'.

Representation contract (shared with the C port):
- None == JSON null; a raise is {"__raise__": cls, "message": str(exc)}.
  BaseException-only kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
  CancelledError) pass through every except-Exception site. The FULL builtin
  hierarchy plus queue.Empty(Exception), CompressionLockHeld(Exception),
  CompressionBusy(Exception), SessionNotOwned(RuntimeError) and
  AuthError(RuntimeError) ride make_exc.
- CompressionLockHeld is built with its live __init__(holder): str(exc) ==
  "Compression lock held: {holder or 'unknown'}"; proj_exc carries holder.
- session is a fixture dict; its "agent" value (null or dict) materializes
  to a SimpleNamespace double (always truthy, even {}). Seam args project
  session with agent -> "agent" marker and history_lock -> its fixture
  marker. history_lock truthy => silent CM double; a MISSING key raises
  KeyError('history_lock') at `with session["history_lock"]`.
- request (parse_compress_args) and _credits_state dicts materialize to
  SimpleNamespaces; missing attributes raise the module-qualified
  'types.SimpleNamespace' AttributeError live.
- Doubles passed through seams stay fixture dicts; .get sites raise the
  live AttributeError on non-dict carriers.
- Tuples project to lists; sets to sorted lists; integral floats to ints;
  Paths to POSIX strings (POSIX fixture scope).
- str()/slicing/title()/lower() fold the ASCII fixture corpus only; output
  constants carry the live non-ASCII glyphs verbatim.
- Fixture domains: usage/route/build/fb/ack/response/info dicts; tool names
  and roles str/int; numbers integral; no NaN/Infinity; history_lock present
  => truthy marker; valid_tool_names a list; reload_mcp_tools present =>
  true; arg/name/command/sid strings.
"""
import ast
import contextlib
import copy
import json
import os as _os_real
import queue as _queue_real
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import Path, PurePath

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

_REPO = Path(__file__).resolve().parent.parent
Q = _os_real.path.expanduser("~/hermes-agent")
SRC = _os_real.path.join(Q, "tui_gateway", "methods_slash.py")

_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    start = node.lineno
    for d in getattr(node, "decorator_list", []):
        start = min(start, d.lineno)
    return "\n".join(src.splitlines()[start - 1:node.end_lineno])


def exec_node(src, node, ns, path):
    text = textwrap.dedent(seg(src, node))
    exec(compile(text, path, "exec", flags=_FLAGS, dont_inherit=True), ns)


def new_module(name):
    mod = types.ModuleType(name)
    sys.modules[name] = mod
    return mod


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


# ── exception classes (shared identity between make_exc and except sites) ──
class CompressionLockHeld(Exception):
    def __init__(self, holder=None):
        self.holder = holder
        super().__init__(f"Compression lock held: {holder or 'unknown'}")


CompressionBusy = type("CompressionBusy", (Exception,), {})
SessionNotOwned = type("SessionNotOwned", (RuntimeError,), {})
AuthError = type("AuthError", (RuntimeError,), {})

EXC_CLASSES = {
    "BaseException": BaseException, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit,
    "CancelledError": __import__("asyncio").CancelledError,
    "StopIteration": StopIteration, "StopAsyncIteration": StopAsyncIteration,
    "ArithmeticError": ArithmeticError, "FloatingPointError": FloatingPointError,
    "OverflowError": OverflowError, "ZeroDivisionError": ZeroDivisionError,
    "AssertionError": AssertionError, "AttributeError": AttributeError,
    "BufferError": BufferError, "EOFError": EOFError,
    "ImportError": ImportError, "ModuleNotFoundError": ModuleNotFoundError,
    "LookupError": LookupError, "IndexError": IndexError, "KeyError": KeyError,
    "MemoryError": MemoryError, "NameError": NameError,
    "UnboundLocalError": UnboundLocalError,
    "OSError": OSError, "BlockingIOError": BlockingIOError,
    "ChildProcessError": ChildProcessError, "ConnectionError": ConnectionError,
    "BrokenPipeError": BrokenPipeError,
    "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError,
    "ConnectionResetError": ConnectionResetError,
    "FileExistsError": FileExistsError, "FileNotFoundError": FileNotFoundError,
    "InterruptedError": InterruptedError, "IsADirectoryError": IsADirectoryError,
    "NotADirectoryError": NotADirectoryError, "PermissionError": PermissionError,
    "ProcessLookupError": ProcessLookupError, "TimeoutError": TimeoutError,
    "ReferenceError": ReferenceError, "RuntimeError": RuntimeError,
    "NotImplementedError": NotImplementedError, "RecursionError": RecursionError,
    "SyntaxError": SyntaxError, "IndentationError": IndentationError,
    "TabError": TabError, "SystemError": SystemError, "TypeError": TypeError,
    "ValueError": ValueError, "UnicodeError": UnicodeError,
    "UnicodeDecodeError": UnicodeDecodeError,
    "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeTranslateError": UnicodeTranslateError,
    "JSONDecodeError": type("JSONDecodeError", (ValueError,), {}),
    "Warning": Warning, "DeprecationWarning": DeprecationWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning,
    "RuntimeWarning": RuntimeWarning, "SyntaxWarning": SyntaxWarning,
    "UserWarning": UserWarning, "FutureWarning": FutureWarning,
    "ImportWarning": ImportWarning, "UnicodeWarning": UnicodeWarning,
    "BytesWarning": BytesWarning, "ResourceWarning": ResourceWarning,
    "EncodingWarning": EncodingWarning,
    "queue.Empty": _queue_real.Empty,
    "CompressionLockHeld": CompressionLockHeld,
    "CompressionBusy": CompressionBusy,
    "SessionNotOwned": SessionNotOwned,
    "AuthError": AuthError,
}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind)
    if cls is None:
        cls = type(kind, (Exception,), {})
    if kind == "CompressionLockHeld":
        return cls(spec.get("holder"))
    if kind in ("UnicodeDecodeError", "UnicodeEncodeError", "UnicodeTranslateError"):
        e = cls.__new__(cls)
        Exception.__init__(e, spec.get("message", ""))
        return e
    return cls(spec.get("message", ""))


def proj_exc(exc):
    name = "queue.Empty" if isinstance(exc, _queue_real.Empty) else type(exc).__name__
    out = {"__raise__": name, "message": str(exc)}
    if isinstance(exc, CompressionLockHeld):
        out["holder"] = exc.holder
    return out


# ── canonical projection (compare parsed JSON, never raw strings) ──
def canon(v):
    def conv(x):
        if isinstance(x, types.SimpleNamespace):
            return conv(vars(x))
        if isinstance(x, PurePath):
            return x.as_posix()
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


# ── per-case script backend (mirrors the C tmm_call exactly) ──
_MISSING = object()


class Ctx:
    def __init__(self, io):
        self.io = io or {}
        self.cursors = {}
        self.trace = []

    def _record_raise(self, name, args, exc):
        self.trace.append({"name": name, "args": args, "ret": proj_exc(exc)})
        raise exc

    def call(self, name, args):
        args = {k: v for k, v in (args or {}).items() if v is not None}
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
        return copy.deepcopy(ret) if isinstance(ret, (dict, list)) else ret


def rec(ctx, name, args_fn=None, build=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        raw = ctx.call(name, args)
        return build(raw) if build else raw
    return f


# ── doubles ──
class _LockCM:
    """history_lock: silent enter/exit; carries its fixture marker."""
    def __init__(self, marker):
        self.marker = marker

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False


class _ScopeCM:
    """profile_runtime_scope / manual_compress_turn: silent CM."""
    def __enter__(self):
        return None

    def __exit__(self, *exc):
        return False


class _DbCM:
    """session_db: enter yields the scripted db marker; exit silent."""
    def __init__(self, raw):
        self.raw = raw

    def __enter__(self):
        return self.raw

    def __exit__(self, *exc):
        return False


class _Msg:
    def __init__(self, ctx):
        self.ctx = ctx

    def as_openai_message(self):
        return self.ctx.call("message.as_openai_message", {})


def msg_list_build(raw, ctx):
    if isinstance(raw, list):
        return [_Msg(ctx) for _ in raw]
    return raw


class _PersistenceCM:
    """persistence_for: enter yields the persistence double or None."""
    def __init__(self, raw, ctx):
        self.raw = raw
        self.ctx = ctx

    def __enter__(self):
        if isinstance(self.raw, dict):
            return types.SimpleNamespace(
                messages=types.SimpleNamespace(
                    read_conversation=rec(
                        self.ctx, "persistence.read_conversation",
                        lambda ref, req: {
                            "transcript_ref": ref,
                            "include_lineage": getattr(req, "include_lineage", None),
                            "include_row_ids": getattr(req, "include_row_ids", None),
                        },
                        build=lambda raw: msg_list_build(raw, self.ctx))))
        return None

    def __exit__(self, *exc):
        return False


class _ConversationRequest:
    def __init__(self, **kw):
        self.__dict__.update(kw)


class _RuntimeRecord:
    def __init__(self, ctx):
        self.ctx = ctx

    def set(self, session):
        return self.ctx.call("runtime_record_set",
                             {"session": proj_live_session(session)})

    def reset(self, token):
        return self.ctx.call("runtime_record_reset", {"token": token})


class _Logger:
    def __init__(self, ctx):
        self.debug = rec(ctx, "logger.debug",
                         lambda msg, exc_info=False: {"msg": msg, "exc_info": exc_info})


class _ProcessRegistry:
    def __init__(self, ctx):
        self.kill_all = rec(ctx, "process_registry.kill_all",
                            lambda source=None: {"source": source})


def agent_double(raw, ctx):
    if not isinstance(raw, dict):
        return raw
    d = types.SimpleNamespace(**copy.deepcopy(raw))
    d._tmm_marker = "agent"
    d._spawn_background_review = rec(
        ctx, "agent.spawn_background_review", lambda **kw: dict(kw))
    if "reload_mcp_tools" in raw:
        d.reload_mcp_tools = rec(ctx, "agent.reload_mcp_tools", lambda: {})
    if isinstance(raw.get("_credits_state"), dict):
        d._credits_state = types.SimpleNamespace(**raw["_credits_state"])
    return d


def proj_val(v):
    if isinstance(v, types.SimpleNamespace) and getattr(v, "_tmm_marker", None):
        return v._tmm_marker
    if isinstance(v, _LockCM):
        return v.marker
    return v


def proj_live_session(session):
    if not isinstance(session, dict):
        return session
    return {k: proj_val(v) for k, v in session.items()}


def marker(agent):
    if agent is None:
        return None
    return getattr(agent, "_tmm_marker", "agent")


def materialize_session(raw, ctx):
    if not isinstance(raw, dict):
        return raw
    s = copy.deepcopy(raw)
    if isinstance(s.get("agent"), dict):
        s["agent"] = agent_double(s["agent"], ctx)
    if s.get("history_lock"):
        s["history_lock"] = _LockCM(raw["history_lock"])
    return s


# ── extraction ──
SRC_TEXT = open(SRC, encoding="utf-8").read()

PORTED = [
    "_format_live_review_output", "_format_live_refine_output", "_usage_route",
    "_usage_on_nous_route", "_usage_nous_fields", "_usage_account_blocks",
    "_format_live_usage_output", "_format_live_usage_tokens",
    "_live_session_messages", "_format_live_history_output",
    "_format_live_context_output", "_format_live_tools_output",
    "_format_live_help_output", "_format_live_model_output",
    "_format_live_status_output", "_live_slash_command_output",
    "_compress_live_with_feedback", "_mirror_approvals", "_mirror_personality",
    "_mirror_fast", "_mirror_reload_mcp", "_mirror_stop", "_mirror_autoprune",
    "_compute_host_slash", "_mirror_slash_side_effects",
]

TABLE_NODES = ("_LIVE_SLASH_OUTPUT", "_SLASH_MIRRORS")


def extract():
    mod = new_module("tmm_oracle_methods_slash")
    ns = mod.__dict__
    ns.update({
        "contextlib": contextlib, "queue": _queue_real,
        "CompressionLockHeld": CompressionLockHeld,
        "CompressionBusy": CompressionBusy,
        "SessionNotOwned": SessionNotOwned,
    })
    seed_constants(SRC_TEXT, ns, SRC)
    for node in ast.parse(SRC_TEXT).body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            if node.name == "register":
                continue
            exec_node(SRC_TEXT, node, ns, SRC)
    # the dispatch tables reference the functions — exec them AFTER the defs
    for node in ast.parse(SRC_TEXT).body:
        if isinstance(node, ast.Assign):
            tgts = [t.id for t in node.targets if isinstance(t, ast.Name)]
            if any(t in TABLE_NODES for t in tgts):
                exec_node(SRC_TEXT, node, ns, SRC)
    required = PORTED + list(TABLE_NODES) + [
        "_ISOLATED_SESSION_READ_COMMANDS", "_CONTEXT_LOCAL_IN_PROCESS",
        "_NO_AGENT_USAGE", "_MUTATES_WHILE_RUNNING", "_FAST_TIERS",
    ]
    missing = [k for k in required if k not in ns]
    if missing:
        print(f"FATAL oracle_tui_methods_misc_flow: methods_slash missing: {missing}")
        sys.exit(2)
    return ns


NS = extract()
REAL = {nm: NS[nm] for nm in PORTED}

# live-name -> seam-name for the ported siblings (strip the leading _)
SIBLING_SEAMS = {nm: nm.lstrip("_") for nm in PORTED}


def sibling_args(live):
    if live in ("_format_live_review_output", "_format_live_refine_output",
                "_format_live_usage_output", "_format_live_history_output",
                "_format_live_context_output", "_format_live_tools_output",
                "_format_live_help_output", "_format_live_status_output"):
        return lambda sid, session, arg: {
            "sid": sid, "session": proj_live_session(session), "arg": arg}
    if live in ("_usage_route", "_usage_nous_fields", "_usage_account_blocks",
                "_format_live_usage_tokens", "_live_session_messages",
                "_format_live_model_output"):
        return lambda session: {"session": proj_live_session(session)}
    if live == "_usage_on_nous_route":
        return lambda route: {"route": route}
    if live in ("_mirror_approvals", "_mirror_personality", "_mirror_fast",
                "_mirror_reload_mcp", "_mirror_stop", "_mirror_autoprune"):
        return lambda sid, session, agent, arg: {
            "sid": sid, "session": proj_live_session(session),
            "agent": marker(agent), "arg": arg}
    if live == "_compress_live_with_feedback":
        return lambda sid, session, agent, arg, *, snapshot_kwargs: {
            "sid": sid, "session": proj_live_session(session),
            "agent": marker(agent), "arg": arg, "snapshot_kwargs": snapshot_kwargs}
    if live == "_compute_host_slash":
        return lambda sid, session, name, command: {
            "sid": sid, "session": proj_live_session(session),
            "name": name, "command": command}
    if live == "_mirror_slash_side_effects":
        return lambda sid, session, command: {
            "sid": sid, "session": proj_live_session(session), "command": command}
    if live == "_live_slash_command_output":
        return lambda sid, session, name, arg: {
            "sid": sid, "session": proj_live_session(session),
            "name": name, "arg": arg}
    raise KeyError(live)


# ── sys.modules stub packages (created once, rebound per case) ──
for _pkg in ("agent", "hermes_cli", "tools", "persistence",
             "persistence.conversation"):
    if _pkg not in sys.modules:
        new_module(_pkg)
    sys.modules[_pkg].__path__ = []

REVIEW_MOD = new_module("agent.review_engine")
BILLING_MOD = new_module("agent.billing_links")
NOUS_MOD = new_module("agent.account_usage_nous")
ACCT_MOD = new_module("agent.account_usage")
BREAKDOWN_MOD = new_module("agent.context_breakdown")
FILESRC_MOD = new_module("agent.context_file_sources")
COMPRESSION_MOD = new_module("agent.conversation_compression")
MANUAL_MOD = new_module("agent.conversation_compression_manual")
FEEDBACK_MOD = new_module("agent.manual_compression_feedback")
METADATA_MOD = new_module("agent.model_metadata")
COMMANDS_MOD = new_module("hermes_cli.commands")
PERSONALITY_MOD = new_module("hermes_cli.personality")
PRUNE_MOD = new_module("hermes_cli.prune_advisor")
PROCREG_MOD = new_module("tools.process_registry")
MESSAGES_MOD = new_module("persistence.conversation.messages")

sys.modules["agent"].review_engine = REVIEW_MOD
sys.modules["agent"].billing_links = BILLING_MOD
sys.modules["agent"].account_usage_nous = NOUS_MOD
sys.modules["agent"].account_usage = ACCT_MOD
sys.modules["agent"].context_breakdown = BREAKDOWN_MOD
sys.modules["agent"].context_file_sources = FILESRC_MOD
sys.modules["agent"].conversation_compression = COMPRESSION_MOD
sys.modules["agent"].conversation_compression_manual = MANUAL_MOD
sys.modules["agent"].manual_compression_feedback = FEEDBACK_MOD
sys.modules["agent"].model_metadata = METADATA_MOD
sys.modules["hermes_cli"].commands = COMMANDS_MOD
sys.modules["hermes_cli"].personality = PERSONALITY_MOD
sys.modules["hermes_cli"].prune_advisor = PRUNE_MOD
sys.modules["tools"].process_registry = PROCREG_MOD
sys.modules["persistence.conversation"].messages = MESSAGES_MOD

MESSAGES_MOD.ConversationRequest = _ConversationRequest
# the live constant, verbatim from agent/conversation_compression_manual.py
MANUAL_MOD.AGGRESSIVE_UNSUPPORTED = (
    "--aggressive is not supported; use '/compress here [N]' to keep only recent exchanges, "
    "or /undo to drop turns.")

_CUR_CTX = None


def _commands_getattr(name):
    if name == "COMMANDS_BY_CATEGORY":
        return _CUR_CTX.call("commands_by_category", {})
    raise AttributeError(f"module 'hermes_cli.commands' has no attribute '{name}'")


COMMANDS_MOD.__getattr__ = _commands_getattr


def _set_session_context_args(session_key, cwd=None, *, ui_session_id=_MISSING):
    args = {"session_key": session_key, "cwd": cwd}
    if ui_session_id is not _MISSING:
        args["ui_session_id"] = ui_session_id
    return args


def install_stubs(ctx, target):
    """Rebind every seam slot for one case."""
    global _CUR_CTX
    _CUR_CTX = ctx
    # ported siblings: restore real, then seam all but the target
    for nm, real in REAL.items():
        NS[nm] = real
    for live, seam in SIBLING_SEAMS.items():
        if live == target:
            continue
        NS[live] = rec(ctx, seam, sibling_args(live))

    # server globals
    NS["_session_uses_compute_host"] = rec(
        ctx, "session_uses_compute_host",
        lambda session, cfg=None: {"session": proj_live_session(session)})
    NS["_set_session_context"] = rec(
        ctx, "set_session_context", _set_session_context_args)
    NS["_clear_session_context"] = rec(
        ctx, "clear_session_context", lambda tokens: {"tokens": tokens})
    NS["_current_runtime_session_record"] = _RuntimeRecord(ctx)
    NS["_session_profile_runtime_scope"] = rec(
        ctx, "profile_runtime_scope",
        lambda session, **kw: {"session": proj_live_session(session)},
        build=lambda raw: _ScopeCM())
    NS["_session_db"] = rec(
        ctx, "session_db",
        lambda session: {"session": proj_live_session(session)},
        build=lambda raw: _DbCM(raw))
    NS["_persistence_for"] = rec(
        ctx, "persistence_for", lambda db: {"db": db},
        build=lambda raw: _PersistenceCM(raw, ctx))
    NS["_deferred_build_agent_kwargs"] = rec(
        ctx, "deferred_build_agent_kwargs",
        lambda current, session_db: {
            "session": proj_live_session(current), "session_db": session_db})
    NS["_resolve_agent_model_runtime"] = rec(
        ctx, "resolve_agent_model_runtime",
        lambda model_override, provider_override: {
            "model_override": model_override, "provider_override": provider_override})
    NS["_session_usage_snapshot"] = rec(
        ctx, "session_usage_snapshot",
        lambda session: {"session": proj_live_session(session)})
    NS["_metadata_mirror"] = rec(
        ctx, "metadata_mirror",
        lambda session: {"session": proj_live_session(session)})
    NS["_history_to_messages"] = rec(
        ctx, "history_to_messages",
        lambda history, *, profile_home=None, image_urls=True: {
            "history": history, "profile_home": profile_home})
    NS["_session_cwd"] = rec(
        ctx, "session_cwd",
        lambda session: {"session": proj_live_session(session)})
    NS["_session_info"] = rec(
        ctx, "session_info",
        lambda agent, session=None: {
            "agent": marker(agent), "session": proj_live_session(session)})
    NS["_methods"] = {
        "session.status": rec(ctx, "methods.session_status",
                              lambda rid, params: {"rid": rid, "params": params}),
    }
    NS["_emit"] = rec(
        ctx, "emit",
        lambda event, sid, payload=None: {
            "event": event, "sid": sid, "payload": payload})
    NS["broadcast_session_info"] = rec(ctx, "broadcast_session_info", lambda: {})
    NS["_load_cfg"] = rec(ctx, "load_cfg", lambda: {})
    NS["_validate_personality"] = rec(
        ctx, "validate_personality",
        lambda value, cfg=None: {"arg": value, "cfg": cfg})
    NS["_apply_personality_to_session"] = rec(
        ctx, "apply_personality_to_session",
        lambda sid, session, new_prompt, pname: {
            "sid": sid, "session": proj_live_session(session),
            "new_prompt": new_prompt, "pname": pname})
    NS["_manual_compress_turn"] = rec(
        ctx, "manual_compress_turn",
        lambda sid, session: {"sid": sid, "session": proj_live_session(session)},
        build=lambda raw: _ScopeCM())
    NS["_compress_session_history"] = rec(
        ctx, "compress_session_history",
        lambda session, arg, **kw: {
            "session": proj_live_session(session), "arg": arg, **kw})
    NS["_sync_session_key_after_compress"] = rec(
        ctx, "sync_session_key_after_compress",
        lambda sid, session: {"sid": sid, "session": proj_live_session(session)})
    NS["_apply_model_switch"] = rec(
        ctx, "apply_model_switch",
        lambda sid, session, arg: {
            "sid": sid, "session": proj_live_session(session), "arg": arg})
    NS["busy_message"] = rec(
        ctx, "busy_message",
        lambda command, compressing=False: {
            "command": command, "compressing": compressing})
    NS["_send_compute_host_control"] = rec(
        ctx, "send_compute_host_control",
        lambda sid, *, route_name, command, wait=False, **kw: {
            "sid": sid, "route_name": route_name, "command": command, "wait": wait})
    NS["_send_claimed_compute_host_control"] = rec(
        ctx, "send_claimed_compute_host_control",
        lambda sid, session, *, route_name, command, compress: {
            "sid": sid, "session": proj_live_session(session),
            "route_name": route_name, "command": command, "compress": compress})
    NS["_apply_compute_host_metadata_mirror"] = rec(
        ctx, "apply_compute_host_metadata_mirror",
        lambda session, frame: {
            "session": proj_live_session(session), "ack": frame})
    NS["logger"] = _Logger(ctx)

    # in-function import modules
    REVIEW_MOD.start_review = rec(
        ctx, "start_review",
        lambda agent, snapshot, arg: {
            "agent": marker(agent), "snapshot": snapshot, "arg": arg})
    REVIEW_MOD.format_dispatch_note = rec(
        ctx, "format_dispatch_note",
        lambda result, arg: {"result": result, "arg": arg})
    BILLING_MOD.is_nous_inference_route = rec(
        ctx, "is_nous_inference_route",
        lambda provider, base_url: {"provider": provider, "base_url": base_url})
    NOUS_MOD.nous_credits_usage_fields = rec(
        ctx, "nous_credits_usage_fields",
        lambda agent, nous_route=None: {
            "agent": marker(agent), "nous_route": nous_route})
    NOUS_MOD.nous_usage_lines = rec(
        ctx, "nous_usage_lines",
        lambda nous_route=None, credits_state=None: {
            "nous_route": nous_route,
            "credits_state": (vars(credits_state)
                              if isinstance(credits_state, types.SimpleNamespace)
                              else credits_state)})
    ACCT_MOD.account_limit_lines = rec(
        ctx, "account_limit_lines",
        lambda provider, base_url=None, api_key=None: {
            "provider": provider, "base_url": base_url, "api_key": api_key})
    BREAKDOWN_MOD.compute_session_context_breakdown = rec(
        ctx, "compute_session_context_breakdown",
        lambda agent, live_messages: {
            "agent": marker(agent), "live_messages": live_messages})
    BREAKDOWN_MOD.compute_context_details = rec(
        ctx, "compute_context_details", lambda agent: {"agent": marker(agent)})
    BREAKDOWN_MOD.render_context_breakdown_lines = rec(
        ctx, "render_context_breakdown_lines",
        lambda payload, details=None, grid=False: {
            "payload": payload, "details": details, "grid": grid})
    FILESRC_MOD.context_file_sources_for_agent = rec(
        ctx, "context_file_sources_for_agent",
        lambda agent: {"agent": marker(agent)})
    FILESRC_MOD.render_context_file_lines = rec(
        ctx, "render_context_file_lines", lambda sources: {"sources": sources})
    COMPRESSION_MOD.finalize_context_engine_compression_notification = rec(
        ctx, "finalize_context_engine_notification",
        lambda agent, committed=None: {
            "agent": marker(agent), "committed": committed})
    MANUAL_MOD.parse_compress_args = rec(
        ctx, "parse_compress_args", lambda arg: {"arg": arg},
        build=lambda raw: types.SimpleNamespace(**raw) if isinstance(raw, dict) else raw)
    MANUAL_MOD.compress_now = rec(
        ctx, "compress_now",
        lambda agent, messages, request: {
            "agent": marker(agent), "preview_messages": messages,
            "request": vars(request) if isinstance(request, types.SimpleNamespace)
            else request})
    MANUAL_MOD.render_compress_result = rec(
        ctx, "render_compress_result", lambda result: {"result": result})
    FEEDBACK_MOD.describe_compression_lock_skip = rec(
        ctx, "describe_compression_lock_skip", lambda holder: {"holder": holder})
    FEEDBACK_MOD.summarize_manual_compression = rec(
        ctx, "summarize_manual_compression",
        lambda before_messages, after_messages, before_tokens, after_tokens,
        compression_state=None: {
            "before_messages": before_messages, "after_messages": after_messages,
            "before_tokens": before_tokens, "after_tokens": after_tokens,
            "compression_state": compression_state})
    METADATA_MOD.estimate_request_tokens_rough = rec(
        ctx, "estimate_request_tokens_rough",
        lambda messages, system_prompt=None, tools=None: {
            "messages": messages, "system_prompt": system_prompt, "tools": tools})
    PERSONALITY_MOD.persist_personality = rec(
        ctx, "persist_personality", lambda pname: {"pname": pname})
    PRUNE_MOD.answered = rec(
        ctx, "prune_advisor.answered",
        lambda home: {"home": home.as_posix() if isinstance(home, PurePath) else home})
    PROCREG_MOD.process_registry = _ProcessRegistry(ctx)

    # re-exec the dispatch tables so direct function refs capture the seams
    for node in ast.parse(SRC_TEXT).body:
        if isinstance(node, ast.Assign):
            tgts = [t.id for t in node.targets if isinstance(t, ast.Name)]
            if any(t in TABLE_NODES for t in tgts):
                exec_node(SRC_TEXT, node, NS, SRC)# ── per-case live drive ──
def drive(func, inp):
    global _CUR_CTX
    args = inp.get("args") or {}
    io = inp.get("io") or {}
    state_in = inp.get("state") or {}
    ctx = Ctx(io)
    install_stubs(ctx, func)
    session = materialize_session(args.get("session"), ctx)
    agent_raw = args.get("agent")
    agent = agent_double(agent_raw, ctx) if isinstance(agent_raw, dict) else agent_raw
    sid = args.get("sid", "")
    arg = args.get("arg", "")
    ret = _MISSING
    try:
        if func in ("_format_live_review_output", "_format_live_refine_output",
                    "_format_live_usage_output", "_format_live_history_output",
                    "_format_live_context_output", "_format_live_tools_output",
                    "_format_live_help_output", "_format_live_status_output"):
            ret = NS[func](sid, session, arg)
        elif func in ("_usage_route", "_usage_nous_fields", "_usage_account_blocks",
                      "_format_live_usage_tokens", "_live_session_messages",
                      "_format_live_model_output"):
            ret = NS[func](session)
        elif func == "_usage_on_nous_route":
            ret = NS[func](args.get("route"))
        elif func == "_live_slash_command_output":
            ret = NS[func](sid, session, args.get("name", ""), arg)
        elif func == "_compress_live_with_feedback":
            ret = NS[func](sid, session, agent, arg,
                           snapshot_kwargs=args.get("snapshot_kwargs", False))
        elif func in ("_mirror_approvals", "_mirror_personality", "_mirror_fast",
                      "_mirror_reload_mcp", "_mirror_stop", "_mirror_autoprune"):
            ret = NS[func](sid, session, agent, arg)
        elif func == "_compute_host_slash":
            ret = NS[func](sid, session, args.get("name", ""), args.get("command", ""))
        elif func == "_mirror_slash_side_effects":
            ret = NS[func](sid, session, args.get("command", ""))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:  # SystemExit included
        ret = proj_exc(exc)
    finally:
        _CUR_CTX = None
    state_out = dict(state_in)
    if func == "_mirror_fast" and agent is not None:
        state_out["service_tier"] = getattr(agent, "service_tier", None)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


# ── three-way seam-name cross-check (C port <-> oracle stubs <-> fixture io) ──
SEAM_NAMES = [
    # ported siblings called from other flows
    "format_live_review_output", "format_live_refine_output",
    "format_live_usage_output", "format_live_usage_tokens",
    "format_live_history_output", "format_live_context_output",
    "format_live_tools_output", "format_live_help_output",
    "format_live_model_output", "format_live_status_output",
    "usage_route", "usage_on_nous_route", "usage_account_blocks",
    "live_session_messages", "compute_host_slash",
    "compress_live_with_feedback", "mirror_approvals", "mirror_personality",
    "mirror_fast", "mirror_reload_mcp", "mirror_stop", "mirror_autoprune",
    "mirror_slash_side_effects",
    # server globals
    "session_uses_compute_host", "set_session_context", "clear_session_context",
    "runtime_record_set", "runtime_record_reset", "profile_runtime_scope",
    "session_db", "persistence_for", "persistence.read_conversation",
    "message.as_openai_message", "deferred_build_agent_kwargs",
    "resolve_agent_model_runtime", "session_usage_snapshot", "metadata_mirror",
    "history_to_messages", "session_cwd", "session_info",
    "methods.session_status", "emit", "broadcast_session_info", "load_cfg",
    "validate_personality", "apply_personality_to_session",
    "manual_compress_turn", "compress_session_history",
    "sync_session_key_after_compress", "apply_model_switch", "busy_message",
    "send_compute_host_control", "send_claimed_compute_host_control",
    "apply_compute_host_metadata_mirror", "logger.debug",
    # in-function imports
    "is_nous_inference_route", "nous_credits_usage_fields", "account_limit_lines",
    "nous_usage_lines", "compute_session_context_breakdown",
    "compute_context_details", "render_context_breakdown_lines",
    "context_file_sources_for_agent", "render_context_file_lines",
    "start_review", "format_dispatch_note", "parse_compress_args",
    "compress_now", "render_compress_result", "estimate_request_tokens_rough",
    "describe_compression_lock_skip", "summarize_manual_compression",
    "finalize_context_engine_notification", "persist_personality",
    "process_registry.kill_all", "prune_advisor.answered",
    "commands_by_category", "agent.spawn_background_review",
    "agent.reload_mcp_tools",
]


def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "tui_methods_misc_flow.c",
                encoding="utf-8").read()
    registry = set(SEAM_NAMES)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_tui_methods_misc_flow: seams absent from C port: {missing_c}")
        sys.exit(2)
    c_seams = set(re.findall(r'tmm_call\(ctx,\s*"([^"]+)"', csrc))
    c_seams |= set(re.findall(r'tmm_sib_(?:fmt3|session|mirror4)\(ctx,\s*"([^"]+)"', csrc))
    unknown_c = c_seams - registry
    if unknown_c:
        print(f"FATAL oracle_tui_methods_misc_flow: C seams outside registry: {sorted(unknown_c)}")
        sys.exit(2)
    osrc = open(__file__, encoding="utf-8").read()
    used = set(re.findall(r'rec\(\s*\n?\s*ctx,\s*"([^"]+)"', osrc))
    used |= {"runtime_record_set", "runtime_record_reset",
             "agent.spawn_background_review", "agent.reload_mcp_tools",
             "commands_by_category", "process_registry.kill_all"}
    unknown = used - registry
    if unknown:
        print(f"FATAL oracle_tui_methods_misc_flow: oracle seams outside registry: {sorted(unknown)}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "tui_methods_misc_flow.jsonl"
    if fix.exists():
        fix_keys = set()
        funcs = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            case = json.loads(line)
            funcs.add(case["func"])
            inp = case.get("in") or {}
            fix_keys |= set(inp.get("io") or {})
        bad = fix_keys - registry
        if bad:
            print(f"FATAL oracle_tui_methods_misc_flow: fixture io keys without a seam: {sorted(bad)}")
            sys.exit(2)
        bad = funcs - set(PORTED)
        if bad:
            print(f"FATAL oracle_tui_methods_misc_flow: fixture funcs not extracted: {sorted(bad)}")
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
    if shown >= 12:
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
        for k in sorted(keys)[:3]:
            print(f"    {k} want={json.dumps(ws.get(k), ensure_ascii=False)[:200]}")
            print(f"    {k} got ={json.dumps(gs.get(k), ensure_ascii=False)[:200]}")


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
    print(f"ORACLE tui_methods_misc_flow: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    """Python-only drive() self-test over every fixture case (no C involved)."""
    import faulthandler
    faulthandler.dump_traceback_later(180, exit=True)
    fix = _REPO / "tests" / "fixtures" / "tui_methods_misc_flow.jsonl"
    n = bad = 0
    counts = {}
    known_names = set(PORTED) | set(SEAM_NAMES)
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
        if isinstance(r, dict) and r.get("__raise__") == "AttributeError":
            msg = str(r.get("message", ""))
            for nm in known_names:
                if f"'{nm}'" in msg:
                    bad += 1
                    print(f"SELFTEST ATTR FAMILY {func} case {n}: {msg}")
                    break
    # hand-computed spot checks (guard against uniform wrong answers)
    spot = drive("_format_live_model_output",
                 {"args": {"session": {"agent": {"model": "m1", "provider": "p1"}}}})
    assert spot["ret"] == "Current model: m1 (p1)" and spot["trace"] == [], spot
    spot = drive("_mirror_approvals",
                 {"args": {"sid": "s1", "session": {}, "agent": None, "arg": "x"},
                  "io": {"broadcast_session_info": None}})
    assert spot["ret"] is None and len(spot["trace"]) == 1, spot
    assert spot["trace"][0]["name"] == "broadcast_session_info", spot
    spot = drive("_live_slash_command_output",
                 {"args": {"sid": "s1", "session": None, "name": "/CLEAR", "arg": ""}})
    assert spot["ret"] == "Screen clear is terminal-only; desktop/TUI chat left unchanged.", spot
    assert spot["trace"] == [], spot
    spot = drive("_usage_on_nous_route",
                 {"args": {"route": {"provider": "nous"}},
                  "io": {"is_nous_inference_route": True}})
    assert spot["ret"] is True, spot
    spot = drive("_mirror_fast",
                 {"args": {"sid": "s1", "session": {}, "agent": {}, "arg": "fast"},
                  "io": {"session_info": {}, "emit": None}})
    assert spot["state"].get("service_tier") == "priority", spot
    spot = drive("_mirror_stop", {"args": {"sid": "s1", "session": {}, "agent": None,
                                           "arg": ""}, "io": {}})
    assert isinstance(spot["ret"], dict) and spot["ret"]["__raise__"] == "RuntimeError", spot
    assert spot["ret"]["message"] == "no script for process_registry.kill_all", spot
    thin = [f for f in counts if counts[f] < 5]
    if thin:
        bad += 1
        print(f"SELFTEST THIN COVERAGE (<5 cases): {thin}")
    missing = [f for f in PORTED if f not in counts]
    if missing:
        bad += 1
        print(f"SELFTEST MISSING FUNCS: {missing}")
    print(f"SELFTEST tui_methods_misc_flow: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    faulthandler.cancel_dump_traceback_later()
    return 1 if bad or n == 0 else 0


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()