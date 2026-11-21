#!/usr/bin/env python3
"""Generate tests/fixtures/feishu_adapter_flow8.jsonl — input cases for the
Feishu adapter card/payload/lifecycle/batch/connection FLOW oracle.

Every case is hand-scripted: in.io maps a seam name to a result (plain JSON
value, an ordered LIST of results, or {"__script__":true,"raise":{type,message}})
that the C universal seam and the oracle's recording stubs replay identically.
Covers every ported branch, ordered multi-seam traces, short-circuit order,
fail-closed missing/exhausted scripts, and the builtin exception hierarchy
(Exception-side swallowed where the live def has try/except, BaseException-side
propagated verbatim).

Run `python tests/gen_feishu_adapter_flow8_cases.py` to regenerate; the script
fails if any target has fewer than 6 cases, a target is missing, or a fixture
io key has no matching C seam.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "feishu_adapter_flow8.jsonl"
CASES = []


def add(func, args, io=None, note=None):
    case = {"func": func, "in": {"args": args, "io": dict(io or {})}}
    if note:
        case["_note"] = note
    CASES.append(case)


def rais(kind, msg):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


def ret(v):
    return {"__script__": True, "ret": v}


def seq(*vals):
    return list(vals)


# ── _build_update_prompt_card (seams: t, card_button, card) ──
UP = {"prompt": "Do it?", "default": "dflt", "prompt_id": 7}
def up_io(tvals=("DH", "Yes", "No", "Title"), bvals=("BY", "BN"), cval="CARD"):
    return {"t": seq(*[ret(v) for v in tvals]),
            "card_button": seq(*[ret(v) for v in bvals]), "card": [ret(cval)]}
add("_build_update_prompt_card", UP, io=up_io())
add("_build_update_prompt_card", {"prompt": "P", "default": "", "prompt_id": 1},
    io=up_io(tvals=("Yes", "No", "Title")))
add("_build_update_prompt_card", {"prompt": "P", "default": None, "prompt_id": 2},
    io=up_io(tvals=("Yes", "No", "Title"), cval=None))
add("_build_update_prompt_card", UP, io={"t": [rais("ValueError", "t down")]})
add("_build_update_prompt_card", UP, io={"t": [ret("DH"), rais("SystemExit", "sx")]})
add("_build_update_prompt_card", UP,
    io={"t": [ret("DH"), ret("Yes")], "card_button": [rais("RuntimeError", "cb down")]})
add("_build_update_prompt_card", UP,
    io={"t": [ret("DH"), ret("Yes"), ret("No"), ret("Title")],
        "card_button": [ret("BY"), ret("BN")], "card": [rais("BaseException", "be")]})
add("_build_update_prompt_card", UP, io={}, note="fail-closed")

# ── _build_resolved_approval_card (seams: t, card) ──
RA = {"choice": "deny", "user_name": "U", "body": "REQ"}
def ra_io(label="L", decided="D", card="C"):
    return {"t": seq(ret(label), ret(decided)), "card": [ret(card)]}
add("_build_resolved_approval_card", RA, io=ra_io())
add("_build_resolved_approval_card", {"choice": "once", "user_name": "U", "body": None}, io=ra_io())
add("_build_resolved_approval_card", {"choice": "session", "user_name": "U", "body": ""}, io=ra_io())
add("_build_resolved_approval_card", {"choice": "weird", "user_name": "U"}, io=ra_io())
add("_build_resolved_approval_card", RA, io={"t": [rais("ValueError", "t down")]})
add("_build_resolved_approval_card", RA, io={"t": [ret("L"), rais("KeyboardInterrupt", "ki")]})
add("_build_resolved_approval_card", RA,
    io={"t": [ret("L"), ret("D")], "card": [rais("RuntimeError", "card down")]})
add("_build_resolved_approval_card", RA, io={}, note="fail-closed")

# ── _build_resolved_update_prompt_card (seams: t, card) ──
RU = {"answer": "y", "user_name": "U"}
def ru_io(word="Yes", title="T", by="BY", card="C"):
    return {"t": seq(ret(word), ret(title), ret(by)), "card": [ret(card)]}
add("_build_resolved_update_prompt_card", RU, io=ru_io())
add("_build_resolved_update_prompt_card", {"answer": "n", "user_name": "U"}, io=ru_io())
add("_build_resolved_update_prompt_card", {"answer": "", "user_name": "U"}, io=ru_io())
add("_build_resolved_update_prompt_card", RU, io={"t": [rais("ValueError", "word down")]})
add("_build_resolved_update_prompt_card", RU, io={"t": [ret("Yes"), rais("SystemExit", "sx")]})
add("_build_resolved_update_prompt_card", RU,
    io={"t": [ret("Yes"), ret("T"), rais("ValueError", "by down")]})
add("_build_resolved_update_prompt_card", RU,
    io={"t": [ret("Yes"), ret("T"), ret("BY")], "card": [rais("BaseException", "be")]})
add("_build_resolved_update_prompt_card", RU, io={}, note="fail-closed")

# ── _build_outbound_payload (seams: markdown_hint, md_post, py_dumps) ──
OC = {"content": "plain text"}
add("_build_outbound_payload", {"content": "plain text", "prefer_post": True},
    io={"md_post": [ret("POST")]})
add("_build_outbound_payload", OC, io={"markdown_hint": [ret(None)], "py_dumps": [ret('{"text": "plain text"}')]})
add("_build_outbound_payload", OC, io={"markdown_hint": [ret({"m": True})], "md_post": [ret("POST")]})
add("_build_outbound_payload", OC, io={"markdown_hint": [rais("ValueError", "re down")]})
add("_build_outbound_payload", OC, io={"markdown_hint": [ret({"m": True})], "md_post": [rais("RuntimeError", "md down")]})
add("_build_outbound_payload", {"content": "x", "prefer_post": True}, io={"md_post": [rais("ValueError", "md down")]})
add("_build_outbound_payload", {"content": "x", "prefer_post": True}, io={}, note="fail-closed")
add("_build_outbound_payload", OC, io={"markdown_hint": [ret(None)], "py_dumps": [ret(None)]})

# ── _build_media_post_payload (seams: md_post, py_dumps) ──
MP = {"caption": "cap", "media_tag": {"tag": "img", "key": "k1"}}
add("_build_media_post_payload", MP,
    io={"md_post": [ret('{"zh_cn": {"content": []}}')], "py_dumps": [ret("OUT1")]})
add("_build_media_post_payload", MP, io={"md_post": [ret("{}")], "py_dumps": [ret("OUT2")]})
add("_build_media_post_payload", MP, io={"md_post": [ret('{"zh_cn": {}}')], "py_dumps": [ret("OUT3")]})
add("_build_media_post_payload", MP, io={"md_post": [ret('{"zh_cn": {"content": [["x"]]}}')], "py_dumps": [ret("OUT4")]})
add("_build_media_post_payload", MP, io={"md_post": [rais("ValueError", "md down")]})
add("_build_media_post_payload", MP,
    io={"md_post": [ret("{}")], "py_dumps": [rais("RuntimeError", "dumps down")]})
add("_build_media_post_payload", MP, io={"md_post": [ret("{}")], "py_dumps": [ret(None)]})
add("_build_media_post_payload", MP, io={}, note="fail-closed")

# ── _cancel_pending_tasks (seams: tasks_values, task_done, task_cancel, gather, tasks_clear) ──
CP_OK = {"tasks_values": [ret(["a", "b"])], "task_done": seq(ret(False), ret(True)),
         "task_cancel": [ret(None)], "gather": [ret(None)], "tasks_clear": [ret(None)]}
add("_cancel_pending_tasks", {"tasks": ["a", "b"]}, io=CP_OK)
add("_cancel_pending_tasks", {"tasks": []}, io={"tasks_values": [ret([])], "tasks_clear": [ret(None)]})
add("_cancel_pending_tasks", {"tasks": ["a"]},
    io={"tasks_values": [ret(["a"])], "task_done": [rais("ValueError", "done down")]})
add("_cancel_pending_tasks", {"tasks": ["a"]},
    io={"tasks_values": [ret(["a"])], "task_done": [ret(False)], "task_cancel": [rais("RuntimeError", "cancel down")]})
add("_cancel_pending_tasks", {"tasks": ["a"]},
    io={"tasks_values": [ret(["a"])], "task_done": [ret(False)], "task_cancel": [ret(None)],
        "gather": [rais("SystemExit", "sx")]})
add("_cancel_pending_tasks", {"tasks": ["a"]},
    io={"tasks_values": [ret(["a"])], "task_done": [ret(False)], "task_cancel": [ret(None)],
        "gather": [ret(None)], "tasks_clear": [rais("ValueError", "clear down")]})
add("_cancel_pending_tasks", {"tasks": ["a"]}, io={}, note="fail-closed")
add("_cancel_pending_tasks", {"tasks": ["a"]},
    io={"tasks_values": [ret(["a"])], "task_done": [ret(False)], "task_cancel": [ret(None)],
        "gather": [ret(None)], "tasks_clear": [ret(None)]})

# ── _disable_websocket_auto_reconnect (seams: auto_reconnect_set, self_setattr) ──
add("_disable_websocket_auto_reconnect", {"ws_client": "C"},
    io={"auto_reconnect_set": [ret(None)], "self_setattr": [ret(None)]})
add("_disable_websocket_auto_reconnect", {"ws_client": None}, io={})
add("_disable_websocket_auto_reconnect", {"ws_client": "C"},
    io={"auto_reconnect_set": [rais("ValueError", "set down")], "self_setattr": [ret(None)]})
add("_disable_websocket_auto_reconnect", {"ws_client": "C"},
    io={"auto_reconnect_set": [rais("SystemExit", "sx")], "self_setattr": [ret(None)]})
add("_disable_websocket_auto_reconnect", {"ws_client": "C"},
    io={"auto_reconnect_set": [ret(None)], "self_setattr": [rais("RuntimeError", "sa down")]})
add("_disable_websocket_auto_reconnect", {"ws_client": "C"}, io={}, note="fail-closed")
add("_disable_websocket_auto_reconnect", {"ws_client": {"id": 1}},
    io={"auto_reconnect_set": [ret(None)], "self_setattr": [ret(None)]})

# ── _clear_webhook_anomaly (seam: anomaly_pop) ──
add("_clear_webhook_anomaly", {"remote_ip": "1.2.3.4"}, io={"anomaly_pop": [ret(None)]})
add("_clear_webhook_anomaly", {"remote_ip": "1.2.3.4"}, io={"anomaly_pop": [ret((3, 200, 0))]})
add("_clear_webhook_anomaly", {"remote_ip": ""}, io={"anomaly_pop": [ret(None)]})
add("_clear_webhook_anomaly", {"remote_ip": "1.2.3.4"}, io={"anomaly_pop": [rais("ValueError", "pop down")]})
add("_clear_webhook_anomaly", {"remote_ip": "1.2.3.4"}, io={"anomaly_pop": [rais("SystemExit", "sx")]})
add("_clear_webhook_anomaly", {"remote_ip": "1.2.3.4"}, io={}, note="fail-closed")

# ── _dedup_persist_lock_or_create (seams: new_lock, self_setattr) ──
add("_dedup_persist_lock_or_create", {}, io={"new_lock": [ret("LOCK")], "self_setattr": [ret(None)]})
add("_dedup_persist_lock_or_create", {"dedup_persist_lock": "EXIST"}, io={})
add("_dedup_persist_lock_or_create", {"dedup_persist_lock": None},
    io={"new_lock": [ret("LOCK2")], "self_setattr": [ret(None)]})
add("_dedup_persist_lock_or_create", {}, io={"new_lock": [rais("ValueError", "lock down")]})
add("_dedup_persist_lock_or_create", {},
    io={"new_lock": [ret("LOCK")], "self_setattr": [rais("RuntimeError", "sa down")]})
add("_dedup_persist_lock_or_create", {}, io={}, note="fail-closed")
add("_dedup_persist_lock_or_create", {"dedup_persist_lock": {"id": 9}}, io={})

# ── _build_event_handler (seams: builder, register, build) ──
EH = {"handler_available": True, "encrypt_key": "ek", "verification_token": "vt"}
def eh_ok():
    return {"builder": [ret("B")], "register": seq(*[ret(i) for i in range(11)]), "build": [ret("H")]}
add("_build_event_handler", EH, io=eh_ok())
add("_build_event_handler", {"handler_available": False}, io={})
add("_build_event_handler", EH, io={"builder": [ret("B")], "register": seq(*[ret(i) for i in range(11)]), "build": [ret(None)]})
add("_build_event_handler", EH, io={"builder": [rais("ValueError", "builder down")]})
add("_build_event_handler", EH, io={"builder": [ret("B")], "register": [rais("RuntimeError", "reg down")]})
add("_build_event_handler", EH,
    io={"builder": [ret("B")], "register": seq(*[ret(i) for i in range(5)], rais("ValueError", "reg5")),
        "build": [ret("H")]})
add("_build_event_handler", EH,
    io={"builder": [ret("B")], "register": seq(*[ret(i) for i in range(11)]), "build": [rais("BaseException", "be")]})
add("_build_event_handler", EH, io={}, note="fail-closed")

# ── _dispatch_inbound_event (seams: is_command, enqueue_text, should_batch_media, enqueue_media, handle_guards) ──
EV_TEXT = {"event": {"message_type": "text", "id": "m1"}}
EV_PHOTO = {"event": {"message_type": "photo", "id": "m2"}}
add("_dispatch_inbound_event", EV_TEXT, io={"is_command": [ret(False)], "enqueue_text": [ret(None)]})
add("_dispatch_inbound_event", EV_TEXT,
    io={"is_command": [ret(True)], "should_batch_media": [ret(True)], "enqueue_media": [ret(None)]})
add("_dispatch_inbound_event", EV_TEXT,
    io={"is_command": [ret(True)], "should_batch_media": [ret(False)], "handle_guards": [ret(None)]})
add("_dispatch_inbound_event", EV_PHOTO,
    io={"should_batch_media": [ret(True)], "enqueue_media": [ret(None)]})
add("_dispatch_inbound_event", EV_PHOTO,
    io={"should_batch_media": [ret(False)], "handle_guards": [ret(None)]})
add("_dispatch_inbound_event", EV_TEXT, io={"is_command": [rais("ValueError", "cmd down")]})
add("_dispatch_inbound_event", EV_PHOTO, io={"should_batch_media": [rais("SystemExit", "sx")]})
add("_dispatch_inbound_event", EV_TEXT, io={"is_command": [ret(False)], "enqueue_text": [rais("RuntimeError", "enq down")]})
add("_dispatch_inbound_event", EV_TEXT, io={}, note="fail-closed")

# ── _delayed_flush (seams: current_task, async_sleep, flush_now, task_map_get, task_map_pop) ──
DF = {"key": "k", "delay": 0.5}
def df_ok(get="T"):
    return {"current_task": [ret("T")], "async_sleep": [ret(None)], "flush_now": [ret(None)],
            "task_map_get": [ret(get)], "task_map_pop": [ret(None)]}
add("_delayed_flush", DF, io=df_ok())
add("_delayed_flush", DF, io=df_ok(get="OTHER"))
add("_delayed_flush", DF, io={"current_task": [ret("T")], "async_sleep": [rais("ValueError", "sleep down")]}, note="fail-closed")
add("_delayed_flush", DF,
    io={"current_task": [ret("T")], "async_sleep": [ret(None)], "flush_now": [rais("RuntimeError", "flush down")],
        "task_map_get": [ret("T")], "task_map_pop": [ret(None)]})
add("_delayed_flush", DF,
    io={"current_task": [ret("T")], "async_sleep": [ret(None)], "flush_now": [ret(None)],
        "task_map_get": [rais("ValueError", "get down")]})
add("_delayed_flush", DF,
    io={"current_task": [ret("T")], "async_sleep": [ret(None)], "flush_now": [ret(None)],
        "task_map_get": [ret("T")], "task_map_pop": [rais("SystemExit", "sx")]})
add("_delayed_flush", DF,
    io={"current_task": [ret(None)], "async_sleep": [ret(None)], "flush_now": [ret(None)],
        "task_map_get": [ret(None)], "task_map_pop": [ret(None)]})
add("_delayed_flush", DF, io={}, note="fail-closed")

# ── _dispatch_text_batch (seam: handle_guards) ──
add("_dispatch_text_batch", {"event": {"id": "m"}}, io={"handle_guards": [ret(None)]})
add("_dispatch_text_batch", {"event": {"id": "m"}}, io={"handle_guards": [ret("OK")]})
add("_dispatch_text_batch", {"event": {}}, io={"handle_guards": [rais("ValueError", "guards down")]})
add("_dispatch_text_batch", {"event": {"id": "m"}}, io={"handle_guards": [rais("SystemExit", "sx")]})
add("_dispatch_text_batch", {"event": {"id": "x"}}, io={"handle_guards": [ret(None)]})
add("_dispatch_text_batch", {"event": {"id": "m"}}, io={}, note="fail-closed")

# ── _connect_with_retry (seams: connect_websocket, connect_webhook, self_setattr, disable_ws_reconnect, stop_webhook_server, logger_warning, async_sleep) ──
def cwr_reset():
    return {"self_setattr": seq(ret(None), ret(None)), "disable_ws_reconnect": [ret(None)],
            "stop_webhook_server": [ret(None)], "logger_warning": [ret(None)], "async_sleep": [ret(None)]}
add("_connect_with_retry", {"connection_mode": "websocket"}, io={"connect_websocket": [ret(None)]})
add("_connect_with_retry", {"connection_mode": "webhook"}, io={"connect_webhook": [ret(None)]})
add("_connect_with_retry", {"connection_mode": "websocket"},
    io=dict({"connect_websocket": seq(rais("ValueError", "down"), ret(None))}, **cwr_reset()))
add("_connect_with_retry", {"connection_mode": "websocket"},
    io={"connect_websocket": [rais("SystemExit", "sx")]})
add("_connect_with_retry", {"connection_mode": "websocket"},
    io=dict({"connect_websocket": seq(rais("ValueError", "d1"), rais("ValueError", "d2"), rais("ValueError", "d3"))},
            **{k: (v * 2 if isinstance(v, list) else v) for k, v in cwr_reset().items()}), note="fail-closed")
add("_connect_with_retry", {"connection_mode": "websocket"},
    io=dict({"connect_websocket": seq(rais("ValueError", "down"), ret(None))},
            **dict(cwr_reset(), logger_warning=[rais("ValueError", "log down")])))
add("_connect_with_retry", {"connection_mode": "websocket"}, io={}, note="fail-closed")
add("_connect_with_retry", {"connection_mode": "websocket"},
    io=dict({"connect_websocket": seq(rais("ValueError", "down"), ret(None))},
            **dict(cwr_reset(), self_setattr=[rais("RuntimeError", "sa down"), ret(None)])))

# ── _connect_websocket (seams: prepare_client, hydrate_bot_identity, new_ws_client, self_setattr, run_in_executor) ──
CW = {"ws_available": True, "loop_obj": {"id": 1}, "app_id": "a", "app_secret": "s", "event_handler": "H"}
def cw_ok():
    return {"prepare_client": [ret("DOM")], "hydrate_bot_identity": [ret(None)],
            "new_ws_client": [ret("CLIENT")],
            "self_setattr": seq(ret(None), ret(None)), "run_in_executor": [ret("FUT")]}
add("_connect_websocket", {"ws_available": False}, io={})
add("_connect_websocket", dict(CW, loop_obj=None), io={"prepare_client": [ret("DOM")]})
add("_connect_websocket", dict(CW, loop_closed=True), io={"prepare_client": [ret("DOM")]})
add("_connect_websocket", CW, io=cw_ok())
add("_connect_websocket", CW, io={"prepare_client": [rais("ValueError", "prep down")]})
add("_connect_websocket", CW,
    io={"prepare_client": [ret("DOM")], "hydrate_bot_identity": [rais("RuntimeError", "hydrate down")]})
add("_connect_websocket", CW,
    io={"prepare_client": [ret("DOM")], "hydrate_bot_identity": [ret(None)], "new_ws_client": [rais("SystemExit", "sx")]})
add("_connect_websocket", CW,
    io=dict(cw_ok(), run_in_executor=[rais("ValueError", "exec down")]))
add("_connect_websocket", CW, io={}, note="fail-closed")

# ── _connect_webhook (seams: prepare_client, hydrate_bot_identity, web_application, add_post, bind_listener, self_setattr) ──
CH = {"webhook_available": True, "webhook_host": "127.0.0.1", "webhook_port": 8080, "webhook_path": "/hook"}
def ch_ok():
    return {"prepare_client": [ret("DOM")], "hydrate_bot_identity": [ret(None)],
            "web_application": [ret("APP")], "add_post": [ret(None)],
            "bind_listener": [ret("RUNNER")], "self_setattr": [ret(None)]}
add("_connect_webhook", {"webhook_available": False}, io={})
add("_connect_webhook", CH, io=ch_ok())
add("_connect_webhook", CH, io={"prepare_client": [rais("ValueError", "prep down")]})
add("_connect_webhook", CH,
    io={"prepare_client": [ret("DOM")], "hydrate_bot_identity": [rais("RuntimeError", "hydrate down")]})
add("_connect_webhook", CH,
    io={"prepare_client": [ret("DOM")], "hydrate_bot_identity": [ret(None)], "web_application": [rais("ValueError", "app down")]})
add("_connect_webhook", CH,
    io={"prepare_client": [ret("DOM")], "hydrate_bot_identity": [ret(None)], "web_application": [ret("APP")],
        "add_post": [rais("SystemExit", "sx")]})
add("_connect_webhook", CH,
    io=dict(ch_ok(), bind_listener=[rais("ValueError", "bind down")]))
add("_connect_webhook", CH,
    io=dict(ch_ok(), self_setattr=[rais("RuntimeError", "sa down")]))
add("_connect_webhook", CH, io={}, note="fail-closed")


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"WROTE {OUT}: {len(CASES)} cases, {len(counts)} funcs")
    low = [f for f, n in counts.items() if n < 6]
    print("funcs with <6 cases:", low)
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    if low:
        raise SystemExit(1)
    try:
        import sys
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import oracle_feishu_adapter_flow8 as o
        bad = set()
        for c in CASES:
            bad |= set(c["in"].get("io") or {}) - o.SEAMED
        if bad:
            print("fixture io keys without a seam:", sorted(bad))
            raise SystemExit(1)
        print("seam keys verified against", len(o.SEAMED), "C seams")
    except ImportError as exc:
        print("oracle import skipped:", exc)


if __name__ == "__main__":
    main()
