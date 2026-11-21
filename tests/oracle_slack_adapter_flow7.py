#!/usr/bin/env python3
"""Oracle for the Slack plugin-adapter FLOW port, wave 7
(src/gateway/platforms/slack_adapter_flow7.c).

Drives the LIVE plugins/platforms/slack/adapter.py UNCLAIMED remainder (16 funcs)
with SCRIPTED seams: every injected/unported callable is a recording stub fed
the same scripts the C harness feeds sa7_call. Compares the DECISION SEQUENCE
(ordered seam events {"name","args","ret"} in "trace") plus the final return
("ret") and echoed state ("state"). Live defs are AST-extracted (quarry import
chain never imported); every exec namespace is a registered sys.modules module.

Fail-closed: missing defs or 0 cases => exit 2. BaseException-only kinds PASS
THROUGH every except-Exception site. Summary line:
'ORACLE slack_adapter_flow7: N cases, M mismatches'.

Representation contract (shared with the C port): see the C port header.
"""
import ast
import asyncio
import copy
import json
import os
import re
import sys
import textwrap
import types
import __future__ as _fut
from pathlib import Path
from typing import Any, Callable, Optional

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stdin.reconfigure(encoding="utf-8", errors="replace")

_REPO = Path(__file__).resolve().parent.parent
os.environ.setdefault("HOME", str(_REPO.parent))
os.environ.setdefault("USERPROFILE", str(_REPO.parent))
os.environ.setdefault("LOCALAPPDATA", str(_REPO.parent / "AppData" / "Local"))
os.environ.setdefault("HERMES_HOME", str(_REPO / ".testbin" / "dbg_home"))

Q = os.path.expanduser("~/hermes-agent")
A_SRC = os.path.join(Q, "plugins", "platforms", "slack", "adapter.py")
_FLAGS = _fut.annotations.compiler_flag


def seg(src, node):
    return "\n".join(src.splitlines()[node.lineno - 1:node.end_lineno])


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


MODULE_FUNCS = ["_apply_yaml_config"]
CLASS_METHODS = [
    "__init__", "_add_reaction", "_agent_view_context_for_event",
    "_apply_bot_mention", "_assistant_suggested_prompts",
    "_authenticate_workspace", "_begin_interaction", "_bot_authored_thread_root",
    "_build_message_event", "_build_model_picker_model_blocks",
    "_build_model_picker_provider_blocks", "_cache_agent_view_context",
    "_cache_assistant_thread_metadata", "_cache_slack_document",
]
# staticmethod in the class; extracted as a plain function
STATIC_METHODS = ["_append_link_unfurls"]
TARGETS = MODULE_FUNCS + CLASS_METHODS + STATIC_METHODS

# Seam names MUST match the C port and the fixture io-key union exactly (SET EQUALITY).
SEAM_NAMES = [
    "super_init", "slack_dedup_ttl", "new_deduplicator", "new_slash_lanes",
    "new_async_lock", "react", "context_channel_id", "agent_view_context_key",
    "is_forward", "extract_text_from_slack_blocks", "elide", "logger.debug",
    "rewrite_known_bang_command", "slack_strict_mention",
    "slack_thread_require_mention", "register_mentioned_thread",
    "apply_yaml_bridge", "new_web_client", "auth_test", "logger.info",
    "warn_group_dm_scopes", "warn_not_bot_token", "warn_inchannel",
    "ack", "event_team_id", "interaction_fields", "is_interactive_user_authorized",
    "logger.warning", "fetch_thread_context", "media_message_type",
    "resolve_user_name", "resolve_channel_name", "set_assistant_thread_title",
    "event_declares_bot_sender", "resolve_user_is_bot", "build_source",
    "humanize_user_mentions", "channel_prompt_with_identity",
    "resolve_channel_skills", "MessageEvent", "download_slack_file_bytes",
    "cache_document_from_bytes_async", "re.sub", "t", "picker_row_suffix",
    "trim_oldest", "workspace_thread_key", "remember_channel_team",
]


def extract_adapter():
    mod = new_module("sa7_oracle_adapter")
    ns = mod.__dict__
    import logging
    ns.update({"asyncio": asyncio, "logging": logging, "os": os, "re": re,
               "json": json, "Any": Any, "Optional": Optional,
               "Callable": Callable, "__file__": A_SRC})
    src = open(A_SRC, encoding="utf-8").read()
    tree = ast.parse(src)
    seed_constants(src, ns, A_SRC)
    ns["SLACK_AVAILABLE"] = False
    for nm in ("MAX_MESSAGE_LENGTH", "_BOT_TS_MAX", "_MENTIONED_THREADS_MAX"):
        ns.setdefault(nm, 5000)
    # module funcs
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in MODULE_FUNCS:
            exec_node(src, node, ns, A_SRC)
    cls = next((n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "SlackAdapter"), None)
    if cls is None:
        print("FATAL oracle_slack_adapter_flow7: SlackAdapter class not found")
        sys.exit(2)
    wanted = set(CLASS_METHODS) | set(STATIC_METHODS)
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in wanted:
            exec_node(src, node, ns, A_SRC)
    missing = [k for k in TARGETS if k not in ns]
    if missing:
        print(f"FATAL oracle_slack_adapter_flow7: adapter missing: {missing}")
        sys.exit(2)
    return ns


NS = extract_adapter()
REAL = {nm: NS[nm] for nm in TARGETS}
_SDT = {
    ".pdf": "application/pdf", ".md": "text/markdown", ".txt": "text/plain",
    ".csv": "text/csv", ".log": "text/plain", ".json": "application/json",
    ".xml": "application/xml", ".yaml": "application/yaml", ".yml": "application/yaml",
    ".toml": "application/toml", ".ini": "text/plain", ".cfg": "text/plain",
    ".zip": "application/zip", ".doc": "application/msword",
    ".xls": "application/vnd.ms-excel", ".ppt": "application/vnd.ms-powerpoint",
    ".docx": "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
    ".xlsx": "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
    ".pptx": "application/vnd.openxmlformats-officedocument.presentationml.presentation",
    ".ts": "text/plain", ".py": "text/plain", ".sh": "text/plain",
}
_TEXT_EXT = {".txt", ".md", ".csv", ".log", ".json", ".xml", ".yaml", ".yml",
             ".toml", ".ini", ".cfg", ".py", ".ts", ".sh", ".c", ".h"}


# ── per-case seam installation ──
def install_case(ctx, state, args):
    # module-level recorders
    NS["super"] = lambda *a, **k: types.SimpleNamespace(
        __init__=rec(ctx, "super_init", lambda config, platform: {
            "config": config, "platform": platform}))
    NS["Platform"] = types.SimpleNamespace(SLACK="slack")
    NS["_slack_dedup_ttl_seconds"] = rec(ctx, "slack_dedup_ttl", lambda: {})
    NS["MessageDeduplicator"] = rec(ctx, "new_deduplicator",
                                     lambda ttl_seconds: {"ttl": ttl_seconds})
    NS["SlashHandoffLanes"] = rec(ctx, "new_slash_lanes", lambda: {})
    NS["asyncio"] = types.SimpleNamespace(Lock=rec(ctx, "new_async_lock", lambda: {}))
    NS["_apply_yaml_bridge"] = rec(
        ctx, "apply_yaml_bridge",
        lambda slack_cfg, bridge: {"slack_cfg": slack_cfg, "bridge": bridge})
    NS["_YAML_BRIDGE"] = materialize(args.get("bridge", []))
    NS["logger"] = LoggerDouble7(ctx)
    NS["_SLACK_UNFURL_BLOCKS_MAX_CHARS"] = state.get("_SLACK_UNFURL_BLOCKS_MAX_CHARS", 6000)
    NS["ELISION_MARKER_MAX_LEN"] = state.get("ELISION_MARKER_MAX_LEN", 115)
    NS["is_forward"] = rec(ctx, "is_forward", lambda att: {"att": att})
    NS["_extract_text_from_slack_blocks"] = rec(
        ctx, "extract_text_from_slack_blocks", lambda blocks: {"blocks": blocks})
    NS["elide"] = rec(ctx, "elide", lambda text, limit: {"text": text, "limit": limit})
    NS["cache_document_from_bytes_async"] = arec(
        ctx, "cache_document_from_bytes_async",
        lambda raw, filename: {"raw_bytes": decode(raw), "filename": filename})
    NS["SUPPORTED_DOCUMENT_TYPES"] = materialize(state.get("SUPPORTED_DOCUMENT_TYPES", _SDT))
    NS["_TEXT_INJECT_EXTENSIONS"] = materialize(state.get("_TEXT_INJECT_EXTENSIONS", sorted(_TEXT_EXT)))
    NS["re"] = types.SimpleNamespace(sub=rec(
        ctx, "re.sub", lambda pattern, repl, string: {
            "pattern": pattern, "display_name": string}))
    NS["MessageType"] = types.SimpleNamespace(COMMAND="command", TEXT="text")
    NS["MessageEvent"] = rec(ctx, "MessageEvent", lambda **kw: dict(kw))
    NS["t"] = rec(ctx, "t", lambda key, **kw: {"key": key, **kw})
    for nm, val in (("_MODEL_PICKER_PROVIDER_ACTION", "hermes_model_provider"),
                    ("_MODEL_PICKER_MODEL_ACTION", "hermes_model_model"),
                    ("_MODEL_PICKER_CANCEL_ACTION", "hermes_model_cancel"),
                    ("_MODEL_PICKER_BACK_ACTION", "hermes_model_back")):
        NS[nm] = val
    m_mpu = new_module("hermes_cli.model_picker_usage")
    m_mpu.picker_row_suffix = rec(ctx, "picker_row_suffix",
                                  lambda provider, model_id: {"provider": provider, "model_id": model_id})
    m_gpb = new_module("gateway.platforms.base")
    m_gpb.resolve_channel_skills = rec(
        ctx, "resolve_channel_skills",
        lambda extra, cid, x: {"extra": extra, "channel_id": cid, "third": x})

    # self (bound methods / seamed siblings)
    self = types.SimpleNamespace()
    for nm in CLASS_METHODS:
        if nm == "__init__":
            continue
        setattr(self, nm, types.MethodType(REAL[nm], self))
    for nm in STATIC_METHODS:
        setattr(self, nm, REAL[nm])
    self._react = arec(ctx, "react", lambda channel, timestamp, emoji, team_id, remove: {
        "channel": channel, "timestamp": timestamp, "emoji": emoji,
        "team_id": team_id, "remove": remove})
    self._context_channel_id = rec(ctx, "context_channel_id", lambda context: {"context": context})
    self._agent_view_context_key = rec(
        ctx, "agent_view_context_key", lambda team_id, user_id: {"team_id": team_id, "user_id": user_id})
    NS["_rewrite_known_bang_command"] = rec(
        ctx, "rewrite_known_bang_command", lambda text: {"text": text})
    self._rewrite_known_bang_command = NS["_rewrite_known_bang_command"]
    self._slack_strict_mention = rec(ctx, "slack_strict_mention", lambda: {})
    self._slack_thread_require_mention = rec(ctx, "slack_thread_require_mention", lambda: {})
    self._register_mentioned_thread = rec(
        ctx, "register_mentioned_thread", lambda thread_ts, team_id: {"thread_ts": thread_ts, "team_id": team_id})
    def _new_web_client(token, proxy_url):
        ctx.call("new_web_client", {"token": token, "proxy_url": proxy_url})
        return types.SimpleNamespace(auth_test=arec(ctx, "auth_test", lambda: {}))
    self._new_web_client = _new_web_client
    self._warn_if_missing_group_dm_scopes = rec(
        ctx, "warn_group_dm_scopes", lambda ar, tn: {"auth_response": ar, "team_name": tn})
    self._warn_if_not_bot_token = rec(
        ctx, "warn_not_bot_token", lambda ar, tn: {"auth_response": ar, "team_name": tn})
    self._warn_if_inchannel_without_flat_reply = rec(
        ctx, "warn_inchannel", lambda tn: {"team_name": tn})
    self._event_team_id = rec(
        ctx, "event_team_id",
        lambda empty, body, install_set: {"body": body, "install_set": install_set})
    self._interaction_fields = rec(
        ctx, "interaction_fields", lambda body, action: {"body": body, "action": action})
    self._is_interactive_user_authorized = rec(
        ctx, "is_interactive_user_authorized", lambda user_id, **kw: {"user_id": user_id, **kw})
    self._fetch_thread_context = arec(
        ctx, "fetch_thread_context", lambda **kw: dict(kw))
    self._media_message_type = rec(
        ctx, "media_message_type", lambda media_types: {"media_types": media_types})
    self._resolve_user_name = arec(
        ctx, "resolve_user_name", lambda user_id, **kw: {"user_id": user_id, **kw})
    self._resolve_channel_name = arec(
        ctx, "resolve_channel_name", lambda channel_id, **kw: {"channel_id": channel_id, **kw})
    self._set_assistant_thread_title = arec(
        ctx, "set_assistant_thread_title",
        lambda cid, ts, txt, **kw: {"channel_id": cid, "thread_ts": ts, "text": txt, **kw})
    self._event_declares_bot_sender = rec(
        ctx, "event_declares_bot_sender", lambda event: {"event": event})
    self._resolve_user_is_bot = arec(
        ctx, "resolve_user_is_bot", lambda user_id, **kw: {"user_id": user_id, **kw})
    self.build_source = materialize_rec(ctx, "build_source", lambda **kw: dict(kw))
    self._humanize_user_mentions = arec(
        ctx, "humanize_user_mentions", lambda text, **kw: {"text": text, **kw})
    self._channel_prompt_with_identity = rec(
        ctx, "channel_prompt_with_identity", lambda cid, tid: {"channel_id": cid, "team_id": tid})
    async def _download_slack_file_bytes(url, **kw):
        v = ctx.call("download_slack_file_bytes", {"url": url, **kw})
        return v.encode("utf-8") if isinstance(v, str) else v
    self._download_slack_file_bytes = _download_slack_file_bytes
    self._trim_oldest_dict_entries = rec(
        ctx, "trim_oldest", lambda entries, cap: {"entries": entries, "max": cap})
    self._workspace_thread_key = rec(
        ctx, "workspace_thread_key", lambda team_id, channel_id, thread_ts: {
            "team_id": team_id, "channel_id": channel_id, "thread_id": thread_ts})
    self._remember_channel_team = rec(
        ctx, "remember_channel_team", lambda channel_id, team_id: {
            "channel_id": channel_id, "team_id": team_id})
    # state-backed attributes (fresh materialized copies: no echo mutation)
    self._team_clients = materialize(state.get("_team_clients", {}))
    self._team_bot_names = materialize(state.get("_team_bot_names", {}))
    self._bot_display_name = materialize(state.get("_bot_display_name"))
    self._team_bot_user_ids = materialize(state.get("_team_bot_user_ids", {}))
    self._bot_user_id = materialize(state.get("_bot_user_id"))
    self._thread_context_cache = materialize(state.get("_thread_context_cache", {}))
    self._agent_view_contexts = materialize(state.get("_agent_view_contexts", {}))
    self._assistant_threads = materialize(state.get("_assistant_threads", {}))
    self._AGENT_VIEW_CONTEXTS_MAX = state.get("_AGENT_VIEW_CONTEXTS_MAX", 5000)
    self._ASSISTANT_THREADS_MAX = state.get("_ASSISTANT_THREADS_MAX", 5000)
    self.config = types.SimpleNamespace(extra=args.get("extra") or {})
    _last_self[0] = self
    return self


def decode(v):
    return v.decode("utf-8") if isinstance(v, bytes) else v


def materialize_rec(ctx, name, args_fn):
    """Recorder whose RETURN is materialized (object doubles) after logging."""
    def f(*a, **k):
        ret = ctx.call(name, args_fn(*a, **k))
        return materialize(ret)
    return f


# ── per-case drive ──
def drive(func, inp):
    args = inp.get("args") or {}
    state = inp.get("state") or {}
    ctx = Ctx(inp.get("io"))
    state_out = copy.deepcopy(state)
    install_case(ctx, state_out, args)
    self = _last_self[0]
    try:
        if func == "__init__":
            s = types.SimpleNamespace()
            NS[func](s, materialize(args.get("config")))
            ret = None
        elif func == "_apply_yaml_config":
            ret = NS[func](materialize(args.get("yaml_cfg")), materialize(args.get("slack_cfg")))
        elif func == "_append_link_unfurls":
            ret = NS[func](args.get("text"), materialize(args.get("slack_attachments")))
        elif func == "_add_reaction":
            ret = asyncio.run(self._add_reaction(
                args.get("channel"), args.get("timestamp"), args.get("emoji"),
                args.get("team_id", "")))
        elif func == "_agent_view_context_for_event":
            ret = self._agent_view_context_for_event(
                materialize(args.get("event")), args.get("team_id"), args.get("user_id"))
        elif func == "_apply_bot_mention":
            ret = self._apply_bot_mention(
                args.get("text"), args.get("original_text"), args.get("command_probe_text"),
                args.get("is_command_text", False), args.get("bot_uid"),
                args.get("thread_ts"), args.get("team_id", ""))
        elif func == "_assistant_suggested_prompts":
            ret = self._assistant_suggested_prompts()
        elif func == "_authenticate_workspace":
            ret = asyncio.run(self._authenticate_workspace(
                args.get("token"), args.get("proxy_url")))
        elif func == "_begin_interaction":
            ack = arec(ctx, "ack")
            ret = asyncio.run(self._begin_interaction(
                ack, materialize(args.get("body")), materialize(args.get("action")),
                args.get("kind"), team_scoped=args.get("team_scoped", True)))
        elif func == "_bot_authored_thread_root":
            ret = asyncio.run(self._bot_authored_thread_root(
                args.get("channel_id"), args.get("thread_ts"),
                args.get("team_id", ""), args.get("chat_type", "")))
        elif func == "_build_message_event":
            ret = asyncio.run(self._build_message_event(
                materialize(args.get("event")),
                text=args.get("text"), original_text=args.get("original_text"),
                command_probe_text=args.get("command_probe_text"),
                is_command_text=args.get("is_command_text", False),
                channel_id=args.get("channel_id"), team_id=args.get("team_id"),
                ts=args.get("ts"), user_id=args.get("user_id"),
                thread_ts=args.get("thread_ts"), is_dm=args.get("is_dm", False),
                media_urls=args.get("media_urls") or [], media_types=args.get("media_types") or [],
                media_text_inlined=args.get("media_text_inlined") or [],
                channel_context=args.get("channel_context"), reply_expected=args.get("reply_expected")))
        elif func == "_build_model_picker_provider_blocks":
            ret = self._build_model_picker_provider_blocks(
                materialize(args.get("providers")), args.get("current_model"),
                args.get("provider_label"))
        elif func == "_build_model_picker_model_blocks":
            ret = self._build_model_picker_model_blocks(
                materialize(args.get("providers")), args.get("provider_slug"))
        elif func == "_cache_agent_view_context":
            ret = self._cache_agent_view_context(materialize(args.get("metadata")))
        elif func == "_cache_assistant_thread_metadata":
            ret = self._cache_assistant_thread_metadata(materialize(args.get("metadata")))
        elif func == "_cache_slack_document":
            ret = asyncio.run(self._cache_slack_document(
                materialize(args.get("f")), args.get("url"), args.get("mimetype"),
                args.get("team_id", "")))
        else:
            raise RuntimeError(f"unknown func {func}")
    except BaseException as exc:
        ret = proj_exc(exc)
    return {"ret": canon(ret), "trace": canon(ctx.trace), "state": canon(state_out)}


_last_self = [None]


# ── seam-name cross-check (SET EQUALITY across C / oracle / fixtures) ──
def cross_check():
    csrc = open(_REPO / "src" / "gateway" / "platforms" / "slack_adapter_flow7.c",
                encoding="utf-8").read()
    ss = set(SEAM_NAMES)
    if len(ss) != len(SEAM_NAMES):
        print("FATAL oracle_slack_adapter_flow7: duplicate SEAM_NAMES")
        sys.exit(2)
    missing_c = [n for n in SEAM_NAMES if f'"{n}"' not in csrc]
    if missing_c:
        print(f"FATAL oracle_slack_adapter_flow7: seams absent from C port: {missing_c}")
        sys.exit(2)
    fix = _REPO / "tests" / "fixtures" / "slack_adapter_flow7.jsonl"
    if fix.exists():
        fix_keys = set()
        for line in open(fix, encoding="utf-8"):
            line = line.strip()
            if line:
                fix_keys |= set((json.loads(line).get("in") or {}).get("io") or {})
        if fix_keys != ss:
            print(f"FATAL oracle_slack_adapter_flow7: fixture io keys != SEAM_NAMES; "
                  f"missing={sorted(ss - fix_keys)} extra={sorted(fix_keys - ss)}")
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
    print(f"ORACLE slack_adapter_flow7: {n} cases, {m} mismatches")
    if n == 0:
        sys.exit(2)
    sys.exit(1 if m else 0)


def selftest():
    fix = _REPO / "tests" / "fixtures" / "slack_adapter_flow7.jsonl"
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
        expect = case.get("expect_raise")
        if expect:
            if not (isinstance(r, dict) and r.get("__raise__") == expect):
                bad += 1
                print(f"SELFTEST EXPECT {func} case {n}: wanted raise {expect}, got {r}")
        elif isinstance(r, dict) and r.get("__raise__") == "RuntimeError" and \
                str(r.get("message", "")).startswith(("no script", "script exhausted")):
            bad += 1
            print(f"SELFTEST SCRIPT GAP {func} case {n}: {r['message']}")
    missing = [f for f in TARGETS if f not in counts]
    if missing:
        bad += 1
        print(f"SELFTEST MISSING FUNCS: {missing}")
    thin = [f for f, c in counts.items() if c < 6]
    if thin:
        bad += 1
        print(f"SELFTEST THIN FUNCS (<6 cases): {thin}")
    print(f"SELFTEST slack_adapter_flow7: {n} cases, {bad} drive/script failures")
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    return 1 if bad or n == 0 else 0


# ── canonical projection ──
def project(v):
    if isinstance(v, types.SimpleNamespace):
        return {"__obj__": {k: project(x) for k, x in vars(v).items()}}
    if isinstance(v, (set, frozenset)):
        return {"__set__": sorted(project(x) for x in v)}
    if isinstance(v, tuple):
        return [project(x) for x in v]
    if isinstance(v, dict):
        if any(isinstance(k, tuple) for k in v):
            return [{"key": [project(x) for x in k], "value": project(val)} for k, val in v.items()]
        return {k: project(x) for k, x in v.items()}
    if isinstance(v, list):
        return [project(x) for x in v]
    if isinstance(v, float) and v.is_integer():
        return int(v)
    if isinstance(v, BaseException):
        return str(v)
    return v


def canon(v):
    return json.loads(json.dumps(project(v), ensure_ascii=False, default=str))


# ── full builtin exception hierarchy (CPython 3.14, quarry) ──
EXC_CLASSES = {
    "ValueError": ValueError, "TypeError": TypeError, "RuntimeError": RuntimeError,
    "KeyError": KeyError, "AttributeError": AttributeError, "ImportError": ImportError,
    "OSError": OSError, "TimeoutError": TimeoutError, "Exception": Exception,
    "SystemExit": SystemExit, "KeyboardInterrupt": KeyboardInterrupt,
    "GeneratorExit": GeneratorExit, "CancelledError": asyncio.CancelledError,
    "LookupError": LookupError, "IndexError": IndexError, "StopIteration": StopIteration,
    "ArithmeticError": ArithmeticError, "ZeroDivisionError": ZeroDivisionError,
    "UnicodeError": UnicodeError, "BufferError": BufferError, "EOFError": EOFError,
    "MemoryError": MemoryError, "NameError": NameError, "ReferenceError": ReferenceError,
    "SyntaxError": SyntaxError, "SystemError": SystemError, "AssertionError": AssertionError,
    "FloatingPointError": FloatingPointError, "OverflowError": OverflowError,
    "ConnectionError": ConnectionError, "ConnectionResetError": ConnectionResetError,
    "BrokenPipeError": BrokenPipeError, "ConnectionAbortedError": ConnectionAbortedError,
    "ConnectionRefusedError": ConnectionRefusedError, "FileNotFoundError": FileNotFoundError,
    "FileExistsError": FileExistsError, "PermissionError": PermissionError,
    "NotADirectoryError": NotADirectoryError, "IsADirectoryError": IsADirectoryError,
    "InterruptedError": InterruptedError, "ProcessLookupError": ProcessLookupError,
    "ChildProcessError": ChildProcessError, "BlockingIOError": BlockingIOError,
    "RecursionError": RecursionError, "NotImplementedError": NotImplementedError,
    "UnboundLocalError": UnboundLocalError, "ModuleNotFoundError": ModuleNotFoundError,
    "IndentationError": IndentationError, "TabError": TabError,
    "UnicodeDecodeError": UnicodeDecodeError, "UnicodeEncodeError": UnicodeEncodeError,
    "UnicodeTranslateError": UnicodeTranslateError, "StopAsyncIteration": StopAsyncIteration,
    "Warning": Warning, "UserWarning": UserWarning, "DeprecationWarning": DeprecationWarning,
    "RuntimeWarning": RuntimeWarning, "SyntaxWarning": SyntaxWarning,
    "FutureWarning": FutureWarning, "ImportWarning": ImportWarning,
    "PendingDeprecationWarning": PendingDeprecationWarning, "ResourceWarning": ResourceWarning,
    "BytesWarning": BytesWarning, "EncodingWarning": EncodingWarning,
    "BaseException": BaseException, "BaseExceptionGroup": BaseExceptionGroup,
    "ExceptionGroup": ExceptionGroup,
}


def proj_exc(exc):
    return {"__raise__": type(exc).__name__, "message": str(exc)}


def make_exc(spec):
    kind = spec.get("type", "RuntimeError")
    cls = EXC_CLASSES.get(kind) or type(kind, (Exception,), {})
    return cls(spec.get("message", ""))


# ── per-case script backend (mirrors the C sa7_call exactly) ──
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
        self.trace.append({"name": name, "args": args, "ret": canon(ret)})
        return ret


def rec(ctx, name, args_fn=None):
    def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        return ctx.call(name, args)
    return f


def arec(ctx, name, args_fn=None):
    async def f(*a, **k):
        args = args_fn(*a, **k) if args_fn else {}
        return ctx.call(name, args)
    return f


def materialize(v):
    if isinstance(v, dict):
        if "__obj__" in v:
            return types.SimpleNamespace(**{k: materialize(x) for k, x in (v["__obj__"] or {}).items()})
        if "__set__" in v:
            return set(materialize(x) for x in v["__set__"])
        if "__tuple__" in v:
            return tuple(materialize(x) for x in v["__tuple__"])
        return {k: materialize(x) for k, x in v.items()}
    if isinstance(v, list):
        return [materialize(x) for x in v]
    return v


class LoggerDouble7:
    def __init__(self, ctx):
        self.ctx = ctx

    def _log(self, msg, *args, **kwargs):
        return self.ctx.call("logger." + "_dummy", {"args": list(args)})

    def info(self, msg, *args, **kwargs):
        return self.ctx.call("logger.info", {"args": list(args)})

    def debug(self, msg, *args, **kwargs):
        return self.ctx.call("logger.debug", {"args": list(args)})

    def warning(self, msg, *args, **kwargs):
        return self.ctx.call("logger.warning", {"args": list(args)})


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--self-test":
        sys.exit(selftest())
    main()
