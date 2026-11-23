#!/usr/bin/env python3
"""Differential oracle for acp_adapter/server.py (gate w6_server_e95893).

Loads the LIVE module by path with its heavy ACP/schema/session imports stubbed
via sys.modules registration and a parent-package bootstrap. The pure history
projection helpers are replayed against the live functions.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class _Attr:
    def __init__(self, **kw):
        self.__dict__.update(kw)


_SCHEMA_NAMES = (
    "AgentCapabilities", "AgentMessageChunk", "AuthenticateResponse", "ClientCapabilities",
    "ForkSessionResponse", "Implementation", "InitializeResponse", "ListSessionsResponse",
    "LoadSessionResponse", "McpServerHttp", "McpServerSse", "McpServerStdio",
    "NewSessionResponse", "PromptCapabilities", "PromptResponse", "ResumeSessionResponse",
    "SessionCapabilities", "SessionForkCapabilities", "SessionInfo", "SessionInfoUpdate",
    "SessionListCapabilities", "SessionMode", "SessionModeState", "SessionModelState",
    "SessionResumeCapabilities", "SetSessionConfigOptionResponse", "SetSessionModeResponse",
    "SetSessionModelResponse", "TextContentBlock", "Usage", "UsageUpdate", "UserMessageChunk",
)
schema = _mod("acp.schema")
for _n in _SCHEMA_NAMES:
    setattr(schema, _n, type(_n, (_Attr,), {}))
_mod("acp", Agent=type("Agent", (object,), {}),
     update_agent_message_text=lambda *a, **k: None,
     update_agent_thought_text=lambda *a, **k: None,
     tool_content=lambda x: x, text_block=lambda x: x)

# Parent packages: stop the real __init__ files from running.
pkg = _mod("acp_adapter")
pkg.__path__ = [Q + "/acp_adapter"]
agent_pkg = _mod("agent")
agent_pkg.__path__ = [Q + "/agent"]
tools_pkg = _mod("tools")
tools_pkg.__path__ = [Q + "/tools"]

_mod("acp_adapter.auth", TERMINAL_SETUP_AUTH_METHOD_ID="***",
     build_auth_methods=lambda *a, **k: [], detect_provider=lambda *a, **k: None)
_mod("acp_adapter.commands", SlashCommandsMixin=type("SlashCommandsMixin", (object,), {}),
     _estimate_tokens=lambda *a, **k: 0)
_mod("acp_adapter.content", PromptBlock=type("PromptBlock", (_Attr,), {}),
     _content_blocks_to_openai_user_content=lambda *a, **k: {}, _extract_text=lambda *a, **k: "")
_mod("acp_adapter.events",
     AssistantMessageIdAllocator=type("AssistantMessageIdAllocator", (object,), {}),
     _build_plan_update_from_todo_result=lambda *a, **k: None,
     _send_update=lambda *a, **k: None, flush_open_tool_calls=lambda *a, **k: 0,
     make_message_cb=lambda *a, **k: None, make_step_cb=lambda *a, **k: None,
     make_thinking_cb=lambda *a, **k: None, make_tool_progress_cb=lambda *a, **k: None)


def _coerce_tool_args(raw):
    if isinstance(raw, str):
        try:
            raw = json.loads(raw)
        except Exception:
            raw = {"raw": raw}
    return raw if isinstance(raw, dict) else {}


_mod("acp_adapter.tools", coerce_tool_args=_coerce_tool_args,
     build_tool_complete=lambda *a, **k: None, build_tool_start=lambda *a, **k: None)
_mod("acp_adapter.model_catalog", build_model_state=lambda *a, **k: None,
     fallback_model_state=lambda *a, **k: None)
_mod("acp_adapter.modes", EDIT_APPROVAL_POLICY_CONFIG_ID="***",
     DEFAULT_EDIT_APPROVAL_POLICY="default", DEFAULT_MODE="default", MODES={},
     MODE_TO_EDIT_APPROVAL_POLICY={}, EDIT_APPROVAL_POLICY_TO_MODE={})
_mod("acp_adapter.permissions", make_approval_callback=lambda *a, **k: None)
_mod("acp_adapter.provenance", session_provenance_meta=lambda *a, **k: None)
_mod("acp_adapter.session", SessionManager=type("SessionManager", (object,), {}),
     SessionState=type("SessionState", (_Attr,), {}),
     _expand_acp_enabled_toolsets=lambda *a, **k: set())

_mod("agent.context_compressor", COMPRESSED_SUMMARY_METADATA_KEY="compressed_summary",
     ContextCompressor=type("ContextCompressor", (object,), {
         "classify_summary_content": classmethod(lambda cls, text: None)}))
_mod("agent.turn_failure_copy",
     CONTENT_REFUSED_DISPLAY_KIND="content_refused",
     CONTENT_REFUSED_METADATA_KEY="content_refused",
     CONTENT_REFUSED_NOTICE="This message was refused by the model's safety filter \u2014 try a different query.")
_mod("agent.interrupt_compat", request_hard_interrupt=lambda *a, **k: None)
_mod("tools.approval_context", reset_hermes_interactive_context=lambda *a, **k: None,
     set_hermes_interactive_context=lambda *a, **k: None)

SRC = Q + "/acp_adapter/server.py"
spec = importlib.util.spec_from_file_location("w6_server_e95893_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w6_server_e95893_live"] = live
spec.loader.exec_module(live)


def _canon(x):
    if x is None: return ("null",)
    if isinstance(x, bool): return ("bool", x)
    if isinstance(x, (int, float)): return ("num", float(x))
    if isinstance(x, str): return ("str", x)
    if isinstance(x, (list, tuple)): return ("list", [_canon(v) for v in x])
    if isinstance(x, dict): return ("obj", sorted((k, _canon(v)) for k, v in x.items()))
    return ("other", repr(x))


def compute(r):
    f = r.get("func")
    if f == "flatten":
        return live._flatten_history_text(r.get("value"))
    if f == "reasoning":
        return live._history_reasoning_text(r.get("message") or {})
    if f == "notice":
        return live._content_refused_notice(r.get("message") or {})
    if f == "attach":
        return live._attach_interrupted_prompt(r.get("interrupted") or "", r.get("guidance") or "")
    if f == "tool_call":
        name, args = live._history_tool_call_name_args(r.get("tool_call") or {})
        return [name, args]
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        want = compute(r)
    except Exception as exc:
        mism += 1
        print("LIVE ERROR %s case=%s: %r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
        continue
    got = r.get("out")
    if _canon(want) != _canon(got):
        mism += 1
        print("MISMATCH %s case=%s C=%r PY=%r" % (r.get("func"), r.get("case"), got, want),
              file=sys.stderr)
print("ORACLE w6_server_e95893: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism else 0)
