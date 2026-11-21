#!/usr/bin/env python3
"""Generate tests/fixtures/slack_adapter_flow14.jsonl (>=6 cases per function).

Every case carries the FULL seam-name set in ``in.io`` (SEAM-NAME SET EQUALITY):
unspecified seams default to null (a valid scripted result), so any branch a
case reaches has a script and the harness never fail-closes on a missing key.
"""
import json, os

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SEAMS = [
    "apply_slack_proxy","assistant_suggested_prompts","assistant_thread_title_enabled",
    "assistant_threads_setSuggestedPrompts","async_web_client","await","bot_authored_thread_root",
    "build_thread_session_key","client_session","config_extra_get","describe_proxy_error","elide",
    "evict_oldest_by_ts","exists","fetch_thread_parent_text","format_mrkdwn","get_client",
    "get_scoped_secret","get_secret","get_session_store","has_active_session_for_thread",
    "import_aiohttp","import_slack_sdk","is_connected","json_dumps","json_loads",
    "load_slack_bot_tokens","logger.debug","logger.error","logger.info","logger.warning","next_",
    "pconfig_extra","pconfig_token","post","re_compile","reaction_trigger_target",
    "register_mentioned_thread","repr","request_never_left","resolve_proxy_url",
    "resolve_slack_user_dm","resp_json","restart_socket_mode","send_error","session_store_method",
    "session_title_method","set_thread_title","slack_json_post","slack_strict_mention",
    "slack_unfurl_kwargs","sleep","socket_ping_pong_stale","socket_transport_connected",
    "standalone_post_kwargs","standalone_post_text","standalone_proxy_kwargs",
    "standalone_send_media","standalone_upload_file","workspace_message_marker",
    "workspace_thread_key",
]
BASE = {n: None for n in SEAMS}

def exc(kind="RuntimeError", msg="boom"):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}

def mk(func, args=None, state=None, io=None):
    merged = dict(BASE)
    if io: merged.update(io)
    return {"func": func, "in": {"args": args or {}, "state": state or {}, "io": merged}}

CASES = []
def add(func, args=None, state=None, io=None):
    CASES.append(mk(func, args, state, io))

S = "_serialize_slack_blocks_for_agent"
SCAL = ["type", "text"]
REC = ["elements", "accessory"]
blk = [{"type": "section", "text": "hello", "elements": [{"type": "t", "text": "x"}]},
       {"type": "rich_text", "elements": []}]
add(S, {"blocks": [], "scalar_keys": SCAL, "recursive_keys": REC})
add(S, {"blocks": [{"type": "rich_text"}, {"type": "table"}], "scalar_keys": SCAL, "recursive_keys": REC})
add(S, {"blocks": blk, "scalar_keys": SCAL, "recursive_keys": REC, "max_chars": 6000},
    io={"json_dumps": "DUMPED", "elide": "ELIDED"})
add(S, {"blocks": blk, "scalar_keys": SCAL, "recursive_keys": REC},
    io={"json_dumps": exc(), "repr": "REPRVIEW", "elide": "EL2"})
add(S, {"blocks": blk, "scalar_keys": SCAL, "recursive_keys": REC},
    io={"json_dumps": exc("SystemExit", "bye")})
add(S, {"blocks": blk, "scalar_keys": SCAL, "recursive_keys": REC},
    io={"json_dumps": "D", "elide": exc()})

P = "_set_assistant_suggested_prompts"
add(P, {"channel_id": "C1"}, {"_app": None})
add(P, {"channel_id": ""}, {"_app": {"x": 1}})
add(P, {"channel_id": "C1"}, {"_app": {"x": 1}}, {"assistant_suggested_prompts": [["t", []]]})
add(P, {"channel_id": "C1", "team_id": "T1", "thread_ts": "123.4"}, {"_app": {"x": 1}},
    {"assistant_suggested_prompts": [["Title", ["p1", "p2"]]]})
add(P, {"channel_id": "C1"}, {"_app": {"x": 1}},
    {"assistant_suggested_prompts": [["Title", ["p1"]]], "get_client": exc()})
add(P, {"channel_id": "C1"}, {"_app": {"x": 1}},
    {"assistant_suggested_prompts": [["Title", ["p1"]]],
     "assistant_threads_setSuggestedPrompts": exc("SystemExit", "stop")})

T = "_set_assistant_thread_title"
baseT = {"channel_id": "C1", "thread_ts": "123.4", "title_source": "hello   world"}
appOn = {"_app": {"x": 1}}
add(T, baseT, {"_app": None})
add(T, {"channel_id": "C1", "thread_ts": "", "title_source": "hi"}, appOn)
add(T, baseT, appOn, {"assistant_thread_title_enabled": False})
add(T, baseT, appOn, {"assistant_thread_title_enabled": True, "workspace_thread_key": ""})
add(T, baseT, dict(appOn, _titled_assistant_threads=["k1"]),
    {"assistant_thread_title_enabled": True, "workspace_thread_key": "k1"})
add(T, {"channel_id": "C1", "thread_ts": "1", "title_source": "/cmd"}, appOn,
    {"assistant_thread_title_enabled": True, "workspace_thread_key": "k2"})
add(T, baseT, appOn, {"assistant_thread_title_enabled": True, "workspace_thread_key": "k3"})
add(T, {"channel_id": "C1", "thread_ts": "1", "title_source": "x" * 100}, appOn,
    {"assistant_thread_title_enabled": True, "workspace_thread_key": "k4"})
add(T, baseT, appOn, {"assistant_thread_title_enabled": True, "workspace_thread_key": "k5",
                      "set_thread_title": exc()})
add(T, baseT, appOn, {"assistant_thread_title_enabled": True, "workspace_thread_key": "k6",
                      "set_thread_title": exc("SystemExit", "stop")})

W = "_should_wake_on_unmentioned_message"
wa = {"event_thread_ts": "123.4", "channel_id": "C1", "user_id": "U1", "is_thread_reply": True}
add(W, {"event_thread_ts": ""})
add(W, wa, {"_bot_message_ts": ["m1"]}, {"workspace_message_marker": "m1"})
add(W, wa, {"_mentioned_threads": ["123.4"]}, {"workspace_message_marker": "m1"})
add(W, wa, {}, {"has_active_session_for_thread": True})
add(W, wa, {}, {"bot_authored_thread_root": True})
add(W, wa, {"_team_bot_user_ids": {"T1": "B1"}},
    {"fetch_thread_parent_text": "please <@B1> run", "slack_strict_mention": False})
add(W, wa, {"_team_bot_user_ids": {"T1": "B1"}},
    {"fetch_thread_parent_text": "please <@B1> run", "slack_strict_mention": True})
add(W, wa)

H = "_slack_api_human_users"
add(H, {}, {"_api_human_users_cache": ["cached"]})
add(H, {}, {}, {"config_extra_get": [[" U1 ", "U2", ""]]})
add(H, {}, {}, {"config_extra_get": None, "get_scoped_secret": "a,b"})
add(H, {}, {}, {"config_extra_get": None, "get_scoped_secret": "a,,b,c"})
add(H, {}, {}, {"config_extra_get": None, "get_scoped_secret": exc()})
add(H, {}, {}, {"config_extra_get": 5})

J = "_slack_json_post"
add(J, {"token": "xoxb", "method": "chat.postMessage", "payload": {"text": "hi"}},
    io={"post": "RESP", "resp_json": {"ok": True, "ts": "1.2"}})
add(J, {"token": "t", "method": "users.info", "payload": {"user": "U1"},
        "req_kw": {"timeout": 5, "proxy": "http://p"}},
    io={"post": "R2", "resp_json": {"ok": False, "error": "bad"}})
add(J, {"token": "t", "method": "m", "payload": {}}, io={"post": exc()})
add(J, {"token": "t", "method": "m", "payload": {}}, io={"post": "R", "resp_json": exc()})
add(J, {"token": "", "method": "", "payload": {}}, io={"post": "R", "resp_json": {}})
add(J, {"token": "t", "method": "reactions.add", "payload": {"a": 1}}, io={"post": "R", "resp_json": 7})

MP = "_slack_mention_patterns"
add(MP, {}, {"_compiled_mention_patterns": ["cached"]})
add(MP, {}, {}, {"config_extra_get": [["bot", "help"]]})
add(MP, {}, {}, {"config_extra_get": "single"})
add(MP, {}, {}, {"config_extra_get": {"a": 1}})
add(MP, {}, {}, {"config_extra_get": None, "get_scoped_secret": [["a","b"]]})
add(MP, {}, {}, {"config_extra_get": None, "get_scoped_secret": "a\nb,c"})
add(MP, {}, {}, {"config_extra_get": [["bot"]], "re_compile": exc("re.error", "bad regex")})
add(MP, {}, {}, {"config_extra_get": [["bot"]], "re_compile": exc("SystemExit", "stop")})

AD = "_slack_message_addressed_to_other_user"
for text, uids in [("", []), ("hello", ["U1"]), ("<@U1> hi", ["U1"]),
                   ("<@U2|name> hi", ["U1"]), ("<!here> hi", ["U1"]),
                   ("  <@U2|name> hi", ["U1", "U2"])]:
    add(AD, {"text": text, "self_uids": uids})

MS = "_slack_message_mentions_self"
for text, uids in [("", ["U1"]), ("hello", ["U1"]), ("<@U1>", ["U1"]),
                   ("<@U1|bot> hi", ["U1"]), ("<@U2> hi", ["U1"]),
                   ("<@U2> and <@U1|bot>", ["U1", "U2"])]:
    add(MS, {"text": text, "self_uids": uids})

RP = "_slack_per_request_proxy_middleware"
add(RP, {"proxy_url": "http://p", "client": {"a": 1}})
add(RP, {"proxy_url": None, "client": {}}, {"next_": "RESULT"})
add(RP, {"proxy_url": "http://p", "client": {"a": 1}}, io={"next_": exc()})
add(RP, {"proxy_url": "http://q", "client": None}, io={"apply_slack_proxy": exc()})
add(RP, {"proxy_url": "", "client": {"b": 2}}, io={"next_": 5})
add(RP, {"proxy_url": "http://r", "client": {"c": 3}}, io={"apply_slack_proxy": exc("SystemExit", "s")})

SC = "_socket_transport_connected"
add(SC, {}, {})
add(SC, {}, {"_handler": {"client": {"is_connected": True}}})
add(SC, {}, {"_handler": {"client": {"is_connected": False}}})
add(SC, {}, {"_handler": {"client": {"is_connected": {"__callable__": "is_connected"}}}},
    {"is_connected": True})
add(SC, {}, {"_handler": {"client": {"is_connected": {"__coroutine__": True}}}},
    {"await": False})
add(SC, {}, {"_handler": {"client": {"is_connected": {"__callable__": "is_connected"}}}},
    {"is_connected": exc()})
add(SC, {}, {"_handler": {"client": {"is_connected": {"__callable__": "is_connected"}}}},
    {"is_connected": exc("SystemExit", "s")})

WL = "_socket_watchdog_loop"
add(WL, {"interval_s": 30}, {"_running": False})
add(WL, {"interval_s": 1}, {"_running": True, "_socket_mode_task": None},
    {"sleep": {"stop": True}})
add(WL, {"interval_s": 1}, {"_running": True, "_socket_mode_task": {"done": True}},
    {"sleep": {"stop": True}})
add(WL, {"interval_s": 1}, {"_running": True, "_socket_mode_task": {"done": False}},
    {"sleep": {"stop": True}, "socket_transport_connected": False})
add(WL, {"interval_s": 1}, {"_running": True, "_socket_mode_task": {"done": False}},
    {"sleep": {"stop": True}, "socket_transport_connected": True, "socket_ping_pong_stale": True})
add(WL, {"interval_s": 1}, {"_running": True, "_socket_mode_task": {"done": False}},
    {"sleep": {"stop": True}, "socket_transport_connected": True, "socket_ping_pong_stale": False})
add(WL, {"interval_s": 1}, {"_running": True, "_socket_mode_task": None},
    {"sleep": [exc(), {"stop": True}]})
add(WL, {"interval_s": 1}, {"_running": True, "_socket_mode_task": None},
    {"sleep": exc("SystemExit", "s")})

SS = "_standalone_send"
add(SS, {"chat_id": "C1", "message": "hi"})
add(SS, {"chat_id": "U1", "message": "hi"},
    io={"pconfig_token": "t", "load_slack_bot_tokens": [["t1"]], "resolve_slack_user_dm": None})
add(SS, {"chat_id": "U1", "message": "hi"},
    io={"pconfig_token": "t", "load_slack_bot_tokens": [["t1"]], "resolve_slack_user_dm": "D1"})
add(SS, {"chat_id": "C1", "message": "hi", "media_files": [["/a.png", False]]},
    io={"pconfig_token": "t", "load_slack_bot_tokens": [["t1"]]})
add(SS, {"chat_id": "C1", "message": ""},
    io={"pconfig_token": "t", "load_slack_bot_tokens": [["t1"]]})
add(SS, {"chat_id": "C1", "message": "hi"},
    io={"pconfig_token": "t", "load_slack_bot_tokens": [["t1"]],
        "format_mrkdwn": "hi", "import_aiohttp": exc("ImportError", "no")})
add(SS, {"chat_id": "C1", "message": "hi"},
    io={"pconfig_token": "t", "load_slack_bot_tokens": [["t1"]], "format_mrkdwn": "hi",
        "standalone_proxy_kwargs": [[{}, {}]], "standalone_post_kwargs": {"text": "hi"},
        "slack_json_post": {"ok": True, "ts": "9.9"}})
add(SS, {"chat_id": "C1", "message": "hi", "wrong_workspace_errors": ["wrong_workspace"]},
    io={"pconfig_token": "t", "load_slack_bot_tokens": [["t1","t2"]], "format_mrkdwn": "hi",
        "standalone_proxy_kwargs": [[{}, {}]], "standalone_post_kwargs": {"text": "hi"},
        "slack_json_post": [[{"ok": False, "error": "wrong_workspace"}, {"ok": True, "ts": "2.2"}]]})
add(SS, {"chat_id": "C1", "message": "hi"},
    io={"pconfig_token": "t", "load_slack_bot_tokens": [["t1"]], "format_mrkdwn": "hi",
        "standalone_proxy_kwargs": exc()})
add(SS, {"chat_id": "C1", "message": "hi"},
    io={"pconfig_token": "t", "load_slack_bot_tokens": [["t1"]], "format_mrkdwn": "hi",
        "standalone_proxy_kwargs": [[{}, {}]], "standalone_post_kwargs": {"text": "hi"},
        "slack_json_post": {"ok": False, "error": "channel_not_found"}})

SM = "_standalone_send_media"
add(SM, {"token": "t", "chat_id": "C1", "media_files": []},
    io={"import_slack_sdk": exc("ImportError", "no")})
add(SM, {"token": "t", "chat_id": "C1", "media_files": []})
add(SM, {"token": "t", "chat_id": "C1", "media_files": [], "formatted": "hello"},
    io={"standalone_post_text": {"ok": True, "ts": "3.3"}})
add(SM, {"token": "t", "chat_id": "C1", "media_files": [], "formatted": "hello"},
    io={"standalone_post_text": {"ok": False, "error": "nope"}})
add(SM, {"token": "t", "chat_id": "C1", "media_files": [], "formatted": "hello"},
    io={"standalone_post_text": exc()})
add(SM, {"token": "t", "chat_id": "C1", "media_files": [["/missing.png", False]]},
    io={})
add(SM, {"token": "t", "chat_id": "C1", "media_files": [["/ok.png", False]], "formatted_caption": "cap"},
    io={"exists": True, "standalone_upload_file": {"message_id": "5.5"}})
add(SM, {"token": "t", "chat_id": "C1", "media_files": [["/ok.png", False]]},
    io={"exists": True, "standalone_upload_file": {"error": "upload failed"}})
add(SM, {"token": "t", "chat_id": "C1", "media_files": [["/ok.png", False]]},
    io={"exists": True, "standalone_upload_file": exc()})

SY = "_synthetic_reaction_event"
ev = {"item": {"channel": "C1", "ts": "1.1"}, "reaction": "thumbsup", "user": "U1"}
add(SY, {"event": ev, "action": "added", "thread_ts": "1.1", "team_id": ""})
add(SY, {"event": ev, "action": "added", "thread_ts": "1.1", "team_id": "T1"})
add(SY, {"event": dict(ev, event_ts="9.9"), "action": "removed", "thread_ts": "", "team_id": "T1"})
add(SY, {"event": ev, "action": "added", "thread_ts": "1.1", "team_id": "T1",
         "emoji_map": {"thumbsup": "+1"}})
add(SY, {"event": ev, "action": "added", "thread_ts": "1.1", "team_id": "T1"},
    io={"reaction_trigger_target": [["D9", None]]})
add(SY, {"event": ev, "action": "added", "thread_ts": "1.1", "team_id": "T1"},
    io={"reaction_trigger_target": [["C9", "2.2"]]})
add(SY, {"event": ev, "action": "added", "thread_ts": "1.1", "team_id": "T1"},
    io={"reaction_trigger_target": exc()})

TW = "_thread_watermark_io"
tw = {"method": "get_session_metadata", "channel_id": "C1", "thread_ts": "1.1",
      "user_id": "U1", "team_id": "T1", "args": [""]}
add(TW, tw)
add(TW, tw, io={"get_session_store": {"get_session_metadata": 1}})
add(TW, tw, io={"get_session_store": {"get_session_metadata": 1}, "build_thread_session_key": ""})
add(TW, tw, io={"get_session_store": {"get_session_metadata": 1}, "build_thread_session_key": "S1",
                "session_store_method": {"v": 1}})
add(TW, dict(tw, args=["x", "y"]), io={"get_session_store": {"get_session_metadata": 1},
    "build_thread_session_key": "S1", "session_store_method": "ok"})
add(TW, tw, io={"get_session_store": {"get_session_metadata": 1}, "build_thread_session_key": exc()})

assert all(len([c for c in CASES if c["func"] == f]) >= 6 for f in set(c["func"] for c in CASES))
out = os.path.join(REPO, "tests", "fixtures", "slack_adapter_flow14.jsonl")
with open(out, "w", encoding="utf-8") as fh:
    for c in CASES:
        fh.write(json.dumps(c, ensure_ascii=False) + "\n")
print("wrote", out, len(CASES), "cases;", len(set(c["func"] for c in CASES)), "funcs")
