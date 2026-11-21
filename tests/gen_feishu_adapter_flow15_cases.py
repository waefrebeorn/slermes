#!/usr/bin/env python3
"""Generate tests/fixtures/feishu_adapter_flow15.jsonl — input cases for the
Feishu adapter remainders FLOW port (flow15).

Every case is hand-scripted: in.io maps a seam name to a result (plain JSON
value, an ordered LIST of results, or {"__script__":true,"raise":{type,message}})
that the C universal seam and the oracle's recording stubs replay identically.
Run `python tests/gen_feishu_adapter_flow15_cases.py` to regenerate.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "feishu_adapter_flow15.jsonl"
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


def rt(*vals):
    return [ret(v) for v in vals]


def sr(ok=True, mid=None, err=None):
    return {"success": ok, "message_id": mid, "error": err}


# ── _target ──
add("_target", {"_fallback": "FB"}, io={"ws_loop": rt("LOOP")})
add("_target", {"_fallback": "FB"}, io={"ws_loop": rt(None)})
add("_target", {"_fallback": "FB"}, io={"ws_loop": rt(False)})
add("_target", {"_fallback": "FB"}, io={"ws_loop": rt(0)})
add("_target", {"_fallback": "FB"}, io={"ws_loop": [rais("ValueError", "bad")]})
add("_target", {"_fallback": "FB"}, io={"ws_loop": [rais("SystemExit", "sx")]})
add("_target", {}, io={}, note="fail-closed")

# ── _EA_ACTION_LABELS ──
add("_EA_ACTION_LABELS", {}, io={"t": rt("Once", "Session", "Always", "Deny")})
add("_EA_ACTION_LABELS", {}, io={"t": rt("", "", "", "")})
add("_EA_ACTION_LABELS", {}, io={"t": [ret("A"), rais("ValueError", "bad")]})
add("_EA_ACTION_LABELS", {}, io={"t": [ret("A"), rais("SystemExit", "sx")]})
add("_EA_ACTION_LABELS", {}, io={"t": rt("approve once", "approve session", "approve always", "deny")})
add("_EA_ACTION_LABELS", {}, io={}, note="fail-closed")

# ── _tenant_get_request ──
TGR = {"uri": "/open-apis/bot/v3/bots/basic_batch"}
OK_TGR = {"base_builder": rt("B"), "http_method": rt("B"), "uri": rt("B"),
          "token_types": rt("B"), "build": rt("REQ")}
add("_tenant_get_request", TGR, io=OK_TGR)
add("_tenant_get_request", dict(TGR, queries=[["bot_ids", "ou1"]]),
    io=dict(OK_TGR, queries=rt("B")))
add("_tenant_get_request", dict(TGR, queries=None), io=OK_TGR)
add("_tenant_get_request", TGR, io=dict(OK_TGR, base_builder=[rais("ValueError", "bad")]))
add("_tenant_get_request", TGR, io=dict(OK_TGR, uri=[rais("ValueError", "bad")]))
add("_tenant_get_request", TGR, io=dict(OK_TGR, build=[rais("SystemExit", "sx")]))
add("_tenant_get_request", TGR, io={}, note="fail-closed")

# ── _tenant_get_raw ──
add("_tenant_get_raw", {"uri": "/u"}, io={"tenant_get_request": rt("REQ"),
     "run_blocking": rt({"raw": {"content": "BODY"}})})
add("_tenant_get_raw", {"uri": "/u"}, io={"tenant_get_request": rt("REQ"),
     "run_blocking": rt({})})
add("_tenant_get_raw", {"uri": "/u"}, io={"tenant_get_request": rt("REQ"),
     "run_blocking": rt({"raw": None})})
add("_tenant_get_raw", {"uri": "/u"}, io={"tenant_get_request": [rais("ValueError", "bad")]})
add("_tenant_get_raw", {"uri": "/u"}, io={"tenant_get_request": rt("REQ"),
     "run_blocking": [rais("ValueError", "bad")]})
add("_tenant_get_raw", {"uri": "/u"}, io={"tenant_get_request": rt("REQ"),
     "run_blocking": [rais("SystemExit", "sx")]})
add("_tenant_get_raw", {"uri": "/u"}, io={}, note="fail-closed")

# ── _write_update_prompt_response ──
WU = {"answer": "yes"}
OK_WU = {"get_hermes_home": rt("/h"), "path_join": rt("/h/.update_response"),
         "path_with_suffix": rt("/h/.update_response.tmp"),
         "write_text": rt(None), "path_replace": rt(None)}
add("_write_update_prompt_response", WU, io=OK_WU)
add("_write_update_prompt_response", WU, io=dict(OK_WU, get_hermes_home=[rais("ValueError", "bad")]))
add("_write_update_prompt_response", WU, io=dict(OK_WU, path_join=[rais("ValueError", "bad")]))
add("_write_update_prompt_response", WU, io=dict(OK_WU, path_with_suffix=[rais("ValueError", "bad")]))
add("_write_update_prompt_response", WU, io=dict(OK_WU, write_text=[rais("ValueError", "bad")]))
add("_write_update_prompt_response", WU, io=dict(OK_WU, path_replace=[rais("SystemExit", "sx")]))
add("_write_update_prompt_response", WU, io={}, note="fail-closed")

# ── delete_message ──
DM = {"chat_id": "C1", "message_id": "M1"}
add("delete_message", dict(DM, has_client=False), io={})
add("delete_message", dict(DM, has_client=True, message_id=""), io={})
add("delete_message", dict(DM, has_client=True),
    io={"build_delete_request": rt("REQ"), "run_blocking": rt({"code": 0}),
        "response_succeeded": rt(True)})
add("delete_message", dict(DM, has_client=True),
    io={"build_delete_request": rt("REQ"), "run_blocking": rt({"code": 1}),
        "response_succeeded": rt(False)})
add("delete_message", dict(DM, has_client=True),
    io={"build_delete_request": [rais("ValueError", "bad")]})
add("delete_message", dict(DM, has_client=True),
    io={"build_delete_request": rt("REQ"), "run_blocking": [rais("ValueError", "bad")]})
add("delete_message", dict(DM, has_client=True),
    io={"build_delete_request": [rais("SystemExit", "sx")]})
add("delete_message", dict(DM, has_client=True), io={}, note="fail-closed")

# ── edit_message ──
EM = {"chat_id": "C1", "message_id": "M1", "content": "  hello  "}
OK_EM = {"format_message": rt("hello"),
         "build_outbound_payload": rt(["text", '{"text":"hello"}']),
         "build_update_body": rt("BODY"), "build_update_request": rt("REQ"),
         "run_blocking": rt({"code": 0}), "finalize_send_result": rt(sr(True, None, None))}
add("edit_message", dict(EM, has_client=False), io={})
add("edit_message", dict(EM, has_client=True), io=OK_EM)
add("edit_message", dict(EM, has_client=True),
    io=dict(OK_EM, build_outbound_payload=rt(["post", '{"post":{}}']),
            build_update_body=rt("BODY", "BODY2"),
            build_update_request=rt("REQ", "REQ2"),
            run_blocking=rt({"code": 0}, {"code": 0}),
            finalize_send_result=rt(sr(False, None, "bad post"), sr(True, None, None)),
            post_invalid_search=rt(True), strip_markdown=rt("plain"),
            json_dumps=rt('{"text":"plain"}')))
add("edit_message", dict(EM, has_client=True),
    io=dict(OK_EM, build_outbound_payload=rt(["post", '{"post":{}}']),
            finalize_send_result=rt(sr(False, None, "bad post")),
            post_invalid_search=rt(False)))
add("edit_message", dict(EM, has_client=True),
    io=dict(OK_EM, build_outbound_payload=[rais("ValueError", "boom")],
            proxy_error=rt(None)))
add("edit_message", dict(EM, has_client=True),
    io=dict(OK_EM, build_outbound_payload=[rais("ValueError", "boom")],
            proxy_error=rt("PROXY")))
add("edit_message", dict(EM, has_client=True),
    io=dict(OK_EM, format_message=[rais("SystemExit", "sx")]))
add("edit_message", dict(EM, has_client=True), io={}, note="fail-closed")

# ── get_chat_info ──
GCI = {"chat_id": "C1"}
OK_GCI = {"cache_get": rt(None), "build_chat_request": rt("REQ"),
          "run_blocking": rt({"success": True, "data": {
              "chat_type": " PRIVATE ", "chat_mode": " P2P ", "name": "Bob"}}),
          "map_chat_type": rt("dm"), "cache_set": rt(None)}
add("get_chat_info", dict(GCI, has_client=False), io={})
add("get_chat_info", dict(GCI, has_client=True),
    io={"cache_get": rt({"chat_id": "C1", "name": "Cached", "type": "group"})})
add("get_chat_info", dict(GCI, has_client=True), io=OK_GCI)
add("get_chat_info", dict(GCI, has_client=True),
    io=dict(OK_GCI, run_blocking=rt({"success": False, "code": 1, "msg": "no"})))
add("get_chat_info", dict(GCI, has_client=True),
    io=dict(OK_GCI, run_blocking=[rais("ValueError", "bad")]))
add("get_chat_info", dict(GCI, has_client=True),
    io=dict(OK_GCI, run_blocking=[rais("SystemExit", "sx")]))
add("get_chat_info", dict(GCI, has_client=True), io={}, note="fail-closed")

# ── on_processing_start ──
OPS = {"message_id": "M1"}
add("on_processing_start", dict(OPS), io={"reactions_enabled": rt(False)})
add("on_processing_start", {"message_id": ""}, io={"reactions_enabled": rt(False)})
add("on_processing_start", dict(OPS),
    io={"reactions_enabled": rt(True), "cache_contains": rt(True)})
add("on_processing_start", dict(OPS),
    io={"reactions_enabled": rt(True), "cache_contains": rt(False),
        "add_reaction": rt("R1"), "cache_store": rt(None)})
add("on_processing_start", dict(OPS),
    io={"reactions_enabled": rt(True), "cache_contains": rt(False), "add_reaction": rt(None)})
add("on_processing_start", dict(OPS),
    io={"reactions_enabled": [rais("ValueError", "bad")]})
add("on_processing_start", dict(OPS), io={}, note="fail-closed")

# ── on_processing_complete ──
OPC = {"message_id": "M1", "outcome": "SUCCESS"}
add("on_processing_complete", dict(OPC), io={"reactions_enabled": rt(False)})
add("on_processing_complete", {"message_id": "", "outcome": "FAILURE"},
    io={"reactions_enabled": rt(False)})
add("on_processing_complete", dict(OPC),
    io={"reactions_enabled": rt(True), "proc_cache_get": rt(None)})
add("on_processing_complete", dict(OPC, outcome="FAILURE"),
    io={"reactions_enabled": rt(True), "proc_cache_get": rt("R1"),
        "remove_reaction": rt(True), "cache_pop": rt(None), "add_reaction": rt("R2")})
add("on_processing_complete", dict(OPC, outcome="FAILURE"),
    io={"reactions_enabled": rt(True), "proc_cache_get": rt("R1"),
        "remove_reaction": rt(False)})
add("on_processing_complete", dict(OPC),
    io={"reactions_enabled": rt(True), "proc_cache_get": [rais("ValueError", "bad")]})
add("on_processing_complete", dict(OPC), io={}, note="fail-closed")

# ── send_document ──
SD = {"chat_id": "C1", "file_path": "/f.pdf"}
add("send_document", dict(SD, caption="cap", file_name="f.pdf", reply_to="R", metadata={"k": 1}),
    io={"send_uploaded_file_message": rt(sr(True, "M1"))})
add("send_document", dict(SD), io={"send_uploaded_file_message": rt(sr(False, None, "no"))})
add("send_document", dict(SD), io={"send_uploaded_file_message": rt(None)})
add("send_document", dict(SD), io={"send_uploaded_file_message": [rais("ValueError", "bad")]})
add("send_document", dict(SD), io={"send_uploaded_file_message": [rais("SystemExit", "sx")]})
add("send_document", dict(SD), io={}, note="fail-closed")

# ── send_animation ──
SA = {"chat_id": "C1", "animation_url": "https://x/a.gif"}
OK_SA = {"download_remote_document": rt(["/f.gif", "animation.gif"]),
         "t_anim": rt("downgraded"), "warning_text": rt("warn"),
         "send_document": rt(sr(True, "M1"))}
add("send_animation", dict(SA, caption="cap"),
    io=dict(OK_SA, t_anim=rt("downgraded cap")))
add("send_animation", dict(SA), io=OK_SA)
add("send_animation", dict(SA),
    io=dict(OK_SA, download_remote_document=[rais("ValueError", "bad")],
            super_send_animation=rt(sr(False, None, "fallback"))))
add("send_animation", dict(SA),
    io=dict(OK_SA, download_remote_document=[rais("SystemExit", "sx")]))
add("send_animation", dict(SA), io=dict(OK_SA, t_anim=[rais("ValueError", "bad")]))
add("send_animation", dict(SA), io=dict(OK_SA, send_document=[rais("SystemExit", "sx")]))
add("send_animation", dict(SA), io={}, note="fail-closed")

# ── qr_register ──
QR = {}
add("qr_register", dict(QR, initial_domain="lark", timeout_seconds=30),
    io={"qr_inner": rt({"app_id": "A", "app_secret": "S", "domain": "lark"})})
add("qr_register", dict(QR), io={"qr_inner": rt(None)})
add("qr_register", dict(QR), io={"qr_inner": [rais("RuntimeError", "net")]})
add("qr_register", dict(QR), io={"qr_inner": [rais("URLError", "url")]})
add("qr_register", dict(QR), io={"qr_inner": [rais("OSError", "os")]})
add("qr_register", dict(QR), io={"qr_inner": [rais("JSONDecodeError", "json")]})
add("qr_register", dict(QR), io={"qr_inner": [rais("TypeError", "other")]})
add("qr_register", dict(QR), io={"qr_inner": [rais("SystemExit", "sx")]})
add("qr_register", dict(QR), io={}, note="fail-closed")

# ── _teardown_ws_thread ──
TD_FULL = {"ws_client_present": True, "ws_has_disconnect": True,
           "loop_present": True, "ws_future_present": True}
OK_TD = {"loop_is_closed": rt(False), "ws_disconnect": rt("D"),
         "close_frame": rt(None), "call_soon_threadsafe": rt(None),
         "all_tasks": rt(["T1", "T2"]), "task_done": rt(False, True),
         "task_cancel": rt(None), "call_later": rt(None), "wait_ws_future": rt(None)}
add("_teardown_ws_thread", {"loop_present": False, "ws_client_present": False,
                            "ws_future_present": False}, io={})
add("_teardown_ws_thread", TD_FULL, io=OK_TD)
add("_teardown_ws_thread", dict(TD_FULL, loop_present=True),
    io=dict(OK_TD, loop_is_closed=rt(True)))
add("_teardown_ws_thread", TD_FULL, io=dict(OK_TD, close_frame=[rais("TimeoutError", "t/o")]))
add("_teardown_ws_thread", TD_FULL, io=dict(OK_TD, close_frame=[rais("SystemExit", "sx")]))
add("_teardown_ws_thread", TD_FULL, io=dict(OK_TD, wait_ws_future=[rais("CancelledError", "c")]))
add("_teardown_ws_thread", TD_FULL, io=dict(OK_TD, wait_ws_future=[rais("SystemExit", "sx")]))
add("_teardown_ws_thread", dict(TD_FULL), io={}, note="fail-closed")


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    counts = {}
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False, separators=(",", ":")) + "\n")
            counts[case["func"]] = counts.get(case["func"], 0) + 1
    print(f"WROTE {OUT}: {len(CASES)} cases")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")


if __name__ == "__main__":
    main()
