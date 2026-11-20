#!/usr/bin/env python3
"""Generate tests/fixtures/api_server_runs_flow2.jsonl.

Case shape: {"func": <harness name>, "in": {...flat args + "state" + "io"...}}.
The C harness (t_port_api_server_runs_flow2.c) and the live oracle
(oracle_api_server_runs_flow2.py) consume the identical "in" keys: the adapter
state mirror ("state"), the scripted seam results ("io") and the call args.

Every one of the 14 ported functions gets >= 7 cases. Covered: normal returns,
empty/None inputs, config-vs-env fallback, fail-closed missing seam scripts,
Exception-family raises at every try site (swallowed), BaseException-family
raises (SystemExit / KeyboardInterrupt / GeneratorExit / CancelledError)
passing through, and state mutations (store cache, background-task set).

Seam scripts in "io" are either a bare value (constant), a list (per-call
queue) or {"__script__":true,"ret":v} (constant list/object). A raise is
scripted as {"__raise__":kind,"message":m}.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "api_server_runs_flow2.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def raise_spec(kind, msg=""):
    return {"__raise__": kind, "message": msg}


SESSION_KEYS = [
    "source", "user_id", "model", "title", "started_at", "ended_at", "end_reason",
    "message_count", "tool_call_count", "input_tokens", "output_tokens",
    "cache_read_tokens", "cache_write_tokens", "reasoning_tokens",
    "estimated_cost_usd", "actual_cost_usd", "api_call_count", "parent_session_id",
    "pinned", "archived", "hidden", "system_prompt",
]


def session_io(session, entry=None):
    """Build the ordered getattr script the live _session_row reads."""
    q = [session.get(k) for k in SESSION_KEYS]
    q.append(session.get("session_id"))
    q.append(session.get("model_config", {}))
    if entry is not None:
        q += [entry.get("last_active"), entry.get("preview"), entry.get("lineage_root_id")]
        # the live code reads entry.lineage_root_id twice when it is not None
        if entry.get("lineage_root_id") is not None:
            q.append(entry.get("lineage_root_id"))
    return {"getattr": q}


# ── 1. listen_address (7) ───────────────────────────────────────────────────
add("listen_address", extra={}, io={
    "getenv": ["127.0.0.1", "8642"], "_coerce_port": 8642})
add("listen_address", extra={"host": "0.0.0.0", "port": 9000}, io={
    "getenv": "127.0.0.1", "_coerce_port": 9000})
add("listen_address", extra={"host": "", "port": None}, io={
    "getenv": ["", "7777"], "_coerce_port": 7777})
add("listen_address", extra={"port": "1234"}, io={
    "getenv": "127.0.0.1", "_coerce_port": 1234})
add("listen_address", extra={}, io={
    "getenv": [raise_spec("RuntimeError", "env unavailable"), "8642"], "_coerce_port": 8642})
add("listen_address", extra={"port": 5}, io={
    "getenv": "127.0.0.1", "_coerce_port": raise_spec("ValueError", "bad port")})
add("listen_address", extra={"host": "::1"}, io={
    "getenv": "127.0.0.1", "_coerce_port": 8642})

# ── 2. content_has_visible_payload (7) ──────────────────────────────────────
add("content_has_visible_payload", content="hello", io={})
add("content_has_visible_payload", content="   \t\n", io={})
add("content_has_visible_payload", content=[], io={})
add("content_has_visible_payload", content=[{"type": "text", "text": "hi"}], io={})
add("content_has_visible_payload", content=[{"type": " text ", "text": "  "}], io={})
add("content_has_visible_payload", content=[{"type": "image_url", "image_url": "x"}], io={})
add("content_has_visible_payload", content=[{"type": "audio", "text": "x"}, 42], io={})

# ── 3. session_row (7) ──────────────────────────────────────────────────────
S1 = {k: i for i, k in enumerate(SESSION_KEYS)}
S1["session_id"] = "sess-1"
S1["model_config"] = {"provider": "openai", "model": "gpt"}
add("session_row", io=session_io(S1))
S2 = dict(S1)
S2.update({"source": "api", "hidden": True, "model_config": {}})
add("session_row", io=session_io(S2))
add("session_row", entry={"last_active": 9, "preview": "hi", "lineage_root_id": None},
    io=session_io(S1, {"last_active": 9, "preview": "hi", "lineage_root_id": None}))
add("session_row", entry={"last_active": 9, "preview": "hi", "lineage_root_id": "root-1"},
    io=session_io(S1, {"last_active": 9, "preview": "hi", "lineage_root_id": "root-1"}))
io = session_io(S1)
io["getattr"][3] = raise_spec("AttributeError", "title missing")
add("session_row", io=io)
io = session_io(S1, {"last_active": 1, "preview": "p", "lineage_root_id": "r"})
io["getattr"][25] = raise_spec("KeyError", "'preview'")
add("session_row", entry={"last_active": 1, "preview": "p", "lineage_root_id": "r"}, io=io)
S3 = {k: None for k in SESSION_KEYS}
S3["session_id"] = None
S3["model_config"] = {}
add("session_row", io=session_io(S3))

# ── 4. normalize_chat_content (7) ───────────────────────────────────────────
add("normalize_chat_content", content=None, io={})
add("normalize_chat_content", content="plain text", io={})
add("normalize_chat_content", content=["a", "b", "c"], io={})
add("normalize_chat_content", content=[
    {"type": "text", "text": "hello"}, {"type": "image_url", "image_url": "u"},
    {"type": "output_text", "text": "world"}], io={})
add("normalize_chat_content", content=[["nested", {"type": "input_text", "text": "deep"}]], io={})
add("normalize_chat_content", content=[{"type": "TEXT", "text": "upper"}, {"text": "no type"}], io={})
add("normalize_chat_content", content="", io={})

# ── 5. room_grant_delegate (7) ──────────────────────────────────────────────
add("room_grant_delegate", name="handle_approval", io={
    "room_grant_call": {"status": 200, "body": {"ok": True}}})
add("room_grant_delegate", name="handle_runs", io={"room_grant_call": None})
add("room_grant_delegate", name="h", io={"room_grant_call": [{"status": 201}, {"status": 202}]})
add("room_grant_delegate", name="h", io={
    "room_grant_call": raise_spec("RuntimeError", "grant failed")})
add("room_grant_delegate", name="h", io={
    "room_grant_call": raise_spec("KeyboardInterrupt", "user abort")})
add("room_grant_delegate", name="h", io={
    "room_grant_call": {"__script__": True, "ret": {"x": 1}}})
add("room_grant_delegate", name="h", io={
    "room_grant_call": raise_spec("SystemExit", "bye")})

# ── 6. run_route_delegate (7) ───────────────────────────────────────────────
add("run_route_delegate", name="handle_stop", io={
    "run_route_call": {"status": 204, "body": None}})
add("run_route_delegate", name="handle_runs", io={"run_route_call": None})
add("run_route_delegate", name="h", io={"run_route_call": ["a", "b"]})
add("run_route_delegate", name="h", io={
    "run_route_call": raise_spec("ValueError", "bad route")})
add("run_route_delegate", name="h", io={
    "run_route_call": raise_spec("GeneratorExit", "gen closed")})
add("run_route_delegate", name="h", io={
    "run_route_call": {"z": 9}})
add("run_route_delegate", name="h", io={
    "run_route_call": raise_spec("CancelledError", "cancelled")})

# ── 7. handle_skills (7) ────────────────────────────────────────────────────
add("handle_skills", io={
    "find_all_skills": [{"name": "a"}],
    "sort_skills": [{"name": "a"}, {"name": "b"}],
    "web.json_response": {"status": 200, "body": "ok"}})
add("handle_skills", io={
    "find_all_skills": [], "sort_skills": [], "web.json_response": {"status": 200}})
add("handle_skills", io={
    "find_all_skills": raise_spec("RuntimeError", "scan failed"),
    "_error_response": {"status": 500, "message": "Failed to enumerate skills"}})
add("handle_skills", io={
    "find_all_skills": [], "sort_skills": raise_spec("ValueError", "sort failed"),
    "_error_response": {"status": 500}})
add("handle_skills", io={
    "find_all_skills": raise_spec("KeyboardInterrupt", "abort")})
add("handle_skills", io={
    "find_all_skills": [], "sort_skills": raise_spec("SystemExit", "bye")})
add("handle_skills", io={
    "find_all_skills": [], "sort_skills": [{"name": "x"}],
    "web.json_response": raise_spec("OSError", "write failed")})

# ── 8. handle_toolsets (7) ──────────────────────────────────────────────────
TUPLES = [["alpha", "Alpha", "desc-a"], ["beta", "Beta", "desc-b"]]
add("handle_toolsets", io={
    "load_config": {"k": 1},
    "_get_platform_tools": ["alpha"],
    "get_nous_subscription_features": {"pro": True},
    "_get_effective_configurable_toolsets": {"__script__": True, "ret": TUPLES},
    "resolve_toolset": [["t2", "t1", "t2"], ["beta"]],
    "_toolset_has_keys": [True, False],
    "web.json_response": {"status": 200}})
add("handle_toolsets", io={
    "load_config": {}, "_get_platform_tools": [],
    "get_nous_subscription_features": {},
    "_get_effective_configurable_toolsets": [],
    "web.json_response": {"status": 200}})
add("handle_toolsets", io={
    "load_config": raise_spec("RuntimeError", "config down"),
    "_error_response": {"status": 500}})
add("handle_toolsets", io={
    "load_config": raise_spec("KeyboardInterrupt", "abort")})
add("handle_toolsets", io={
    "load_config": {}, "_get_platform_tools": ["alpha"],
    "get_nous_subscription_features": {},
    "_get_effective_configurable_toolsets": {"__script__": True, "ret": TUPLES},
    "resolve_toolset": [raise_spec("ValueError", "bad toolset"), ["beta"]],
    "_toolset_has_keys": [False],
    "web.json_response": {"status": 200}})
add("handle_toolsets", io={
    "load_config": {}, "_get_platform_tools": [],
    "get_nous_subscription_features": {},
    "_get_effective_configurable_toolsets": {"__script__": True, "ret": TUPLES},
    "resolve_toolset": [raise_spec("SystemExit", "bye")]})
add("handle_toolsets", io={
    "load_config": {}, "_get_platform_tools": [],
    "get_nous_subscription_features": {},
    "_get_effective_configurable_toolsets": {"__script__": True, "ret": TUPLES},
    "resolve_toolset": [["x"], ["y"]],
    "_toolset_has_keys": [raise_spec("RuntimeError", "keys failed")],
    "_error_response": {"status": 500}})

# ── 9. ensure_store (7) ─────────────────────────────────────────────────────
add("ensure_store", state={"gateway_runner": {"g": 1}}, io={
    "get_hermes_home": "/home/a", "selected_session_db": {"db": "selected"}})
add("ensure_store", state={"gateway_runner": {"g": 1}}, io={
    "get_hermes_home": raise_spec("RuntimeError", "home failed")})
add("ensure_store", state={"_session_db": {"db": "cached"}}, io={})
add("ensure_store", state={}, io={
    "get_hermes_home": "/home/b", "_open_and_cache_session_db": {"db": "opened"}})
add("ensure_store", state={}, io={
    "get_hermes_home": "/home/b",
    "_open_and_cache_session_db": raise_spec("RuntimeError", "open failed")})
add("ensure_store", state={}, io={
    "get_hermes_home": "/home/b",
    "_open_and_cache_session_db": raise_spec("KeyboardInterrupt", "abort")})
add("ensure_store", state={}, io={
    "get_hermes_home": raise_spec("ValueError", "home bad")})

# ── 10. store_beside (7) ────────────────────────────────────────────────────
add("store_beside", db=None, io={})
add("store_beside", db={"db": "x"}, io={"adapter_persistence": {"store": "P"}})
add("store_beside", db="handle", io={"adapter_persistence": None})
add("store_beside", db=0, io={"adapter_persistence": {"n": 0}})
add("store_beside", db={"db": "x"}, io={
    "adapter_persistence": raise_spec("RuntimeError", "no adapter")})
add("store_beside", db={"db": "x"}, io={
    "adapter_persistence": raise_spec("KeyboardInterrupt", "abort")})
add("store_beside", db=[1, 2], io={"adapter_persistence": [1, 2]})

# ── 11. current_response_store (7) ──────────────────────────────────────────
add("current_response_store", state={
    "_response_store_home": "/home/same", "_response_store": {"id": "current"}}, io={
    "get_hermes_home": "/home/same"})
add("current_response_store", state={
    "_response_store_home": "", "_response_store": None, "_response_stores": {}}, io={
    "get_hermes_home": "/home/new", "ResponseStore": {"id": "new"}})
add("current_response_store", state={
    "_response_store_home": "", "_response_stores": {"/home/cached": {"id": "cached"}}}, io={
    "get_hermes_home": "/home/cached"})
add("current_response_store", state={"_response_store_home": ""}, io={
    "get_hermes_home": raise_spec("RuntimeError", "home down")})
add("current_response_store", state={"_response_store_home": "", "_response_stores": {}}, io={
    "get_hermes_home": "/home/x", "ResponseStore": raise_spec("OSError", "cannot open")})
add("current_response_store", state={"_response_store_home": "/home/same", "_response_store": None}, io={
    "get_hermes_home": "/home/same"})
add("current_response_store", state={"_response_store_home": "", "_response_stores": {}}, io={
    "get_hermes_home": "relative/path", "ResponseStore": {"id": "rel"}})

# ── 12. check_run_auth (7) ──────────────────────────────────────────────────
add("check_run_auth", request={"path": "/v1/runs"}, permission="status", io={
    "runs._check_run_auth": {"status": 200}})
add("check_run_auth", request={}, permission="stop", io={"runs._check_run_auth": None})
add("check_run_auth", request={"path": "/x"}, permission="status", io={
    "runs._check_run_auth": [{"status": 403}]})
add("check_run_auth", request={}, permission="status", io={
    "runs._check_run_auth": raise_spec("RuntimeError", "auth failed")})
add("check_run_auth", request={}, permission="status", io={
    "runs._check_run_auth": raise_spec("KeyboardInterrupt", "abort")})
add("check_run_auth", request={}, permission="status", io={
    "runs._check_run_auth": {"__script__": True, "ret": {"allow": False}}})
add("check_run_auth", request={}, permission="status", io={
    "runs._check_run_auth": raise_spec("SystemExit", "bye")})

# ── 13. request_owns_run (7) ────────────────────────────────────────────────
add("request_owns_run", request={"path": "/x"}, run_id="r1", io={
    "runs._request_owns_run": True})
add("request_owns_run", request={}, run_id="r2", io={"runs._request_owns_run": False})
add("request_owns_run", request={}, run_id="r3", io={"runs._request_owns_run": None})
add("request_owns_run", request={}, run_id="r4", io={
    "runs._request_owns_run": raise_spec("ValueError", "bad id")})
add("request_owns_run", request={}, run_id="r5", io={
    "runs._request_owns_run": raise_spec("KeyboardInterrupt", "abort")})
add("request_owns_run", request={}, run_id="r6", io={
    "runs._request_owns_run": [True, False]})
add("request_owns_run", request={}, run_id="r7", io={
    "runs._request_owns_run": raise_spec("CancelledError", "cancelled")})

# ── 14. track_background_task (7) ───────────────────────────────────────────
add("track_background_task", task="task-1", state={"_background_tasks": []}, io={
    "bg.add": None, "bg.add_done_callback": None})
add("track_background_task", task="task-2", state={"_background_tasks": ["task-0"]}, io={
    "bg.add": None, "bg.add_done_callback": None})
add("track_background_task", task="task-3", state={"_background_tasks": []}, io={
    "bg.add": raise_spec("TypeError", "unhashable"), "bg.add_done_callback": None})
add("track_background_task", task="task-4", tolerate_missing=True,
    state={"_background_tasks": []}, io={
        "bg.add": raise_spec("AttributeError", "no set")})
add("track_background_task", task="task-5", state={"_background_tasks": []}, io={
    "bg.add": raise_spec("KeyboardInterrupt", "abort")})
add("track_background_task", task="task-6", state={"_background_tasks": []}, io={
    "bg.add": None, "bg.add_done_callback": raise_spec("ValueError", "bad cb")})
add("track_background_task", task="task-7", state={"_background_tasks": []}, io={
    "bg.add": None, "bg.add_done_callback": raise_spec("SystemExit", "bye")})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        for case in CASES:
            fh.write(json.dumps(case, ensure_ascii=False, separators=(",", ":")) + "\n")
    counts = {}
    for case in CASES:
        counts[case["func"]] = counts.get(case["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for func in sorted(counts):
        print("  %-32s %d" % (func, counts[func]))


if __name__ == "__main__":
    main()
