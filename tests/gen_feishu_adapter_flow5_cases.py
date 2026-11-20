#!/usr/bin/env python3
"""Generate tests/fixtures/feishu_adapter_flow5.jsonl — input cases for the
Feishu adapter Lark-API request-builder / send-result FLOW oracle.

Every case is hand-scripted: in.io maps a seam name to a result (plain JSON
value, an ordered LIST of results, or {"__script__":true,"raise":{type,message}})
that the C universal seam and the oracle's recording stubs replay identically.
The corpus covers every ported branch and the full builtin exception hierarchy
(Exception-side and BaseException-side raises all propagate verbatim, since
none of these defs has a try/except).

Run `python tests/gen_feishu_adapter_flow5_cases.py` to regenerate; the script
fails if any target has fewer than 6 cases or a target is missing entirely.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "feishu_adapter_flow5.jsonl"
CASES = []


def add(func, args, io=None):
    CASES.append({"func": func, "in": {"args": args, "io": dict(io or {})}})


def rais(kind, msg):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


def ret(v):
    return {"__script__": True, "ret": v}


def build_io(result=None, exc=None):
    io = {"_sdk_build": []}
    if exc:
        io["_sdk_build"] = [rais(exc[0], exc[1])]
    else:
        io["_sdk_build"] = [ret(result)]
    return io


# ── 13 request builders ──
add("_build_get_message_request", {"message_id": "om_1"},
    io=build_io({"kind": "get_message", "id": "om_1"}))
add("_build_get_message_request", {"message_id": ""}, io=build_io(None))
add("_build_get_message_request", {"message_id": "om_2"}, io=build_io("REQUEST"))
add("_build_get_message_request", {"message_id": "om_3"},
    io={"_sdk_build": [ret({"a": 1}), ret({"a": 2})]})
add("_build_get_message_request", {"message_id": "om_4"}, io=build_io(exc=("ValueError", "bad id")))
add("_build_get_message_request", {"message_id": "om_5"}, io=build_io(exc=("SystemExit", "sx")))
add("_build_get_message_request", {"message_id": "om_6"}, io=build_io(exc=("TypeError", "t")))
add("_build_get_message_request", {"message_id": "om_7"}, io=build_io(exc=("KeyboardInterrupt", "ki")))

add("_build_message_resource_request",
    {"message_id": "om_1", "file_key": "fk_1", "resource_type": "image"},
    io=build_io({"kind": "resource"}))
add("_build_message_resource_request",
    {"message_id": "", "file_key": "", "resource_type": ""}, io=build_io(None))
add("_build_message_resource_request",
    {"message_id": "om_2", "file_key": "fk_2", "resource_type": "file"}, io=build_io("REQ"))
add("_build_message_resource_request",
    {"message_id": "om_3", "file_key": "fk_3", "resource_type": "media"},
    io={"_sdk_build": [ret({"a": 1}), ret({"a": 2})]})
add("_build_message_resource_request",
    {"message_id": "om_4", "file_key": "fk_4", "resource_type": "image"},
    io=build_io(exc=("RuntimeError", "sdk down")))
add("_build_message_resource_request",
    {"message_id": "om_5", "file_key": "fk_5", "resource_type": "image"},
    io=build_io(exc=("GeneratorExit", "ge")))
add("_build_message_resource_request",
    {"message_id": "om_6", "file_key": "fk_6", "resource_type": "file"},
    io=build_io(exc=("OSError", "os")))

add("_build_get_application_request", {"app_id": "cli_a", "lang": "en_us"},
    io=build_io({"kind": "app"}))
add("_build_get_application_request", {"app_id": "", "lang": ""}, io=build_io(None))
add("_build_get_application_request", {"app_id": "cli_b", "lang": "zh_cn"}, io=build_io("APP"))
add("_build_get_application_request", {"app_id": "cli_c", "lang": "ja_jp"},
    io={"_sdk_build": [ret({"x": 1}), ret({"x": 2})]})
add("_build_get_application_request", {"app_id": "cli_d", "lang": "en_us"},
    io=build_io(exc=("LookupError", "lookup")))
add("_build_get_application_request", {"app_id": "cli_e", "lang": "en_us"},
    io=build_io(exc=("BaseException", "be")))
add("_build_get_application_request", {"app_id": "cli_f", "lang": "en_us"},
    io=build_io(exc=("AttributeError", "attr")))

add("_build_reply_message_body",
    {"content": "{\"text\":\"hi\"}", "msg_type": "text", "reply_in_thread": True,
     "uuid_value": "u-1"}, io=build_io({"kind": "reply_body"}))
add("_build_reply_message_body",
    {"content": "", "msg_type": "", "reply_in_thread": False, "uuid_value": ""},
    io=build_io(None))
add("_build_reply_message_body",
    {"content": "c", "msg_type": "post", "reply_in_thread": True, "uuid_value": "u-2"},
    io=build_io("BODY"))
add("_build_reply_message_body",
    {"content": "c", "msg_type": "text", "reply_in_thread": False, "uuid_value": "u-3"},
    io={"_sdk_build": [ret({"a": 1}), ret({"a": 2})]})
add("_build_reply_message_body",
    {"content": "c", "msg_type": "text", "reply_in_thread": True, "uuid_value": "u-4"},
    io=build_io(exc=("ValueError", "bad body")))
add("_build_reply_message_body",
    {"content": "c", "msg_type": "text", "reply_in_thread": True, "uuid_value": "u-5"},
    io=build_io(exc=("SystemExit", "sx")))
add("_build_reply_message_body",
    {"content": "c", "msg_type": "text", "reply_in_thread": None, "uuid_value": "u-6"},
    io=build_io({"ok": True}))

add("_build_reply_message_request", {"message_id": "om_1", "request_body": {"b": 1}},
    io=build_io({"kind": "reply_req"}))
add("_build_reply_message_request", {"message_id": "", "request_body": None}, io=build_io(None))
add("_build_reply_message_request", {"message_id": "om_2", "request_body": "BODY"},
    io=build_io("REQ"))
add("_build_reply_message_request", {"message_id": "om_3", "request_body": [1, 2]},
    io={"_sdk_build": [ret({"a": 1}), ret({"a": 2})]})
add("_build_reply_message_request", {"message_id": "om_4", "request_body": {"b": 2}},
    io=build_io(exc=("TypeError", "bad req")))
add("_build_reply_message_request", {"message_id": "om_5", "request_body": {"b": 3}},
    io=build_io(exc=("KeyboardInterrupt", "ki")))
add("_build_reply_message_request", {"message_id": "om_6", "request_body": 7},
    io=build_io(exc=("RuntimeError", "rt")))

add("_build_update_message_body", {"msg_type": "text", "content": "c"},
    io=build_io({"kind": "update_body"}))
add("_build_update_message_body", {"msg_type": "", "content": ""}, io=build_io(None))
add("_build_update_message_body", {"msg_type": "post", "content": "c2"}, io=build_io("BODY"))
add("_build_update_message_body", {"msg_type": "text", "content": "c3"},
    io={"_sdk_build": [ret({"a": 1}), ret({"a": 2})]})
add("_build_update_message_body", {"msg_type": "text", "content": "c4"},
    io=build_io(exc=("ValueError", "v")))
add("_build_update_message_body", {"msg_type": "text", "content": "c5"},
    io=build_io(exc=("SystemExit", "sx")))
add("_build_update_message_body", {"msg_type": "text", "content": "c6"},
    io=build_io(exc=("OSError", "o")))

add("_build_update_message_request", {"message_id": "om_1", "request_body": {"b": 1}},
    io=build_io({"kind": "update_req"}))
add("_build_update_message_request", {"message_id": "", "request_body": None}, io=build_io(None))
add("_build_update_message_request", {"message_id": "om_2", "request_body": "BODY"},
    io=build_io("REQ"))
add("_build_update_message_request", {"message_id": "om_3", "request_body": {"b": 2}},
    io={"_sdk_build": [ret({"a": 1}), ret({"a": 2})]})
add("_build_update_message_request", {"message_id": "om_4", "request_body": {"b": 3}},
    io=build_io(exc=("TypeError", "t")))
add("_build_update_message_request", {"message_id": "om_5", "request_body": {"b": 4}},
    io=build_io(exc=("GeneratorExit", "ge")))
add("_build_update_message_request", {"message_id": "om_6", "request_body": {"b": 5}},
    io=build_io(exc=("LookupError", "l")))

add("_build_delete_message_request", {"message_id": "om_1"},
    io=build_io({"kind": "delete"}))
add("_build_delete_message_request", {"message_id": ""}, io=build_io(None))
add("_build_delete_message_request", {"message_id": "om_2"}, io=build_io("REQ"))
add("_build_delete_message_request", {"message_id": "om_3"},
    io={"_sdk_build": [ret({"a": 1}), ret({"a": 2})]})
add("_build_delete_message_request", {"message_id": "om_4"},
    io=build_io(exc=("RuntimeError", "rt")))
add("_build_delete_message_request", {"message_id": "om_5"},
    io=build_io(exc=("SystemExit", "sx")))
add("_build_delete_message_request", {"message_id": "om_6"},
    io=build_io(exc=("ValueError", "v")))

add("_build_create_message_body",
    {"receive_id": "ou_1", "msg_type": "text", "content": "c", "uuid_value": "u-1"},
    io=build_io({"kind": "create_body"}))
add("_build_create_message_body",
    {"receive_id": "", "msg_type": "", "content": "", "uuid_value": ""}, io=build_io(None))
add("_build_create_message_body",
    {"receive_id": "ou_2", "msg_type": "post", "content": "c2", "uuid_value": "u-2"},
    io=build_io("BODY"))
add("_build_create_message_body",
    {"receive_id": "ou_3", "msg_type": "text", "content": "c3", "uuid_value": "u-3"},
    io={"_sdk_build": [ret({"a": 1}), ret({"a": 2})]})
add("_build_create_message_body",
    {"receive_id": "ou_4", "msg_type": "text", "content": "c4", "uuid_value": "u-4"},
    io=build_io(exc=("ValueError", "v")))
add("_build_create_message_body",
    {"receive_id": "ou_5", "msg_type": "text", "content": "c5", "uuid_value": "u-5"},
    io=build_io(exc=("BaseException", "be")))
add("_build_create_message_body",
    {"receive_id": "ou_6", "msg_type": "text", "content": "c6", "uuid_value": "u-6"},
    io=build_io(exc=("OSError", "o")))

add("_build_create_message_request", {"receive_id_type": "open_id", "request_body": {"b": 1}},
    io=build_io({"kind": "create_req"}))
add("_build_create_message_request", {"receive_id_type": "", "request_body": None}, io=build_io(None))
add("_build_create_message_request", {"receive_id_type": "chat_id", "request_body": "BODY"},
    io=build_io("REQ"))
add("_build_create_message_request", {"receive_id_type": "user_id", "request_body": {"b": 2}},
    io={"_sdk_build": [ret({"a": 1}), ret({"a": 2})]})
add("_build_create_message_request", {"receive_id_type": "open_id", "request_body": {"b": 3}},
    io=build_io(exc=("TypeError", "t")))
add("_build_create_message_request", {"receive_id_type": "open_id", "request_body": {"b": 4}},
    io=build_io(exc=("KeyboardInterrupt", "ki")))
add("_build_create_message_request", {"receive_id_type": "open_id", "request_body": {"b": 5}},
    io=build_io(exc=("RuntimeError", "r")))

add("_build_image_upload_body", {"image_type": "message", "image": "B64"},
    io=build_io({"kind": "image_body"}))
add("_build_image_upload_body", {"image_type": "", "image": None}, io=build_io(None))
add("_build_image_upload_body", {"image_type": "message", "image": {"bytes": 3}},
    io=build_io("BODY"))
add("_build_image_upload_body", {"image_type": "message", "image": [1, 2]},
    io={"_sdk_build": [ret({"a": 1}), ret({"a": 2})]})
add("_build_image_upload_body", {"image_type": "message", "image": "B64"},
    io=build_io(exc=("ValueError", "v")))
add("_build_image_upload_body", {"image_type": "message", "image": "B64"},
    io=build_io(exc=("SystemExit", "sx")))
add("_build_image_upload_body", {"image_type": "message", "image": "B64"},
    io=build_io(exc=("OSError", "o")))

add("_build_image_upload_request", {"request_body": {"b": 1}},
    io=build_io({"kind": "image_req"}))
add("_build_image_upload_request", {"request_body": None}, io=build_io(None))
add("_build_image_upload_request", {"request_body": "BODY"}, io=build_io("REQ"))
add("_build_image_upload_request", {"request_body": [1]},
    io={"_sdk_build": [ret({"a": 1}), ret({"a": 2})]})
add("_build_image_upload_request", {"request_body": {"b": 2}},
    io=build_io(exc=("TypeError", "t")))
add("_build_image_upload_request", {"request_body": {"b": 3}},
    io=build_io(exc=("GeneratorExit", "ge")))
add("_build_image_upload_request", {"request_body": {"b": 4}},
    io=build_io(exc=("LookupError", "l")))

add("_build_file_upload_request", {"request_body": {"b": 1}},
    io=build_io({"kind": "file_req"}))
add("_build_file_upload_request", {"request_body": None}, io=build_io(None))
add("_build_file_upload_request", {"request_body": "BODY"}, io=build_io("REQ"))
add("_build_file_upload_request", {"request_body": [1]},
    io={"_sdk_build": [ret({"a": 1}), ret({"a": 2})]})
add("_build_file_upload_request", {"request_body": {"b": 2}},
    io=build_io(exc=("ValueError", "v")))
add("_build_file_upload_request", {"request_body": {"b": 3}},
    io=build_io(exc=("SystemExit", "sx")))
add("_build_file_upload_request", {"request_body": {"b": 4}},
    io=build_io(exc=("OSError", "o")))

# ── _response_succeeded (pure, response object) ──
add("_response_succeeded", {"response": None})
add("_response_succeeded", {"response": {}})
add("_response_succeeded", {"response": {"success": True, "code": 0, "msg": "ok"}})
add("_response_succeeded", {"response": {"success": False, "code": 1, "msg": "bad"}})
add("_response_succeeded", {"response": {"success": 0}})
add("_response_succeeded", {"response": {"success": "yes"}})
add("_response_succeeded", {"response": {"success": []}})
add("_response_succeeded", {"response": {"success": 1, "data": {"message_id": "m1"}}})

# ── _extract_response_field (one _response_succeeded seam) ──
add("_extract_response_field",
    {"response": {"success": True, "data": {"message_id": "m1"}}, "field_name": "message_id"},
    io={"_response_succeeded": [ret(True)]})
add("_extract_response_field",
    {"response": {"success": True, "data": {"message_id": "m1"}}, "field_name": "missing"},
    io={"_response_succeeded": [ret(True)]})
add("_extract_response_field",
    {"response": {"success": True, "data": None}, "field_name": "message_id"},
    io={"_response_succeeded": [ret(True)]})
add("_extract_response_field",
    {"response": {"success": True}, "field_name": "message_id"},
    io={"_response_succeeded": [ret(True)]})
add("_extract_response_field",
    {"response": {"success": False, "data": {"message_id": "m1"}}, "field_name": "message_id"},
    io={"_response_succeeded": [ret(False)]})
add("_extract_response_field",
    {"response": None, "field_name": "message_id"},
    io={"_response_succeeded": [ret(False)]})
add("_extract_response_field",
    {"response": {"success": True, "data": {"message_id": "m1"}}, "field_name": "message_id"},
    io={"_response_succeeded": [rais("ValueError", "gate down")]})
add("_extract_response_field",
    {"response": {"success": True, "data": {"message_id": "m2", "code": 0}}, "field_name": "code"},
    io={"_response_succeeded": [ret(1)]})

# ── _response_error_result (pure, returns SendResult) ──
add("_response_error_result",
    {"response": {"code": 14, "msg": "rate limited"}, "default_message": "fail"},
    io=None)
add("_response_error_result",
    {"response": {"code": 0, "msg": "ok"}, "default_message": "fail", "override_error": "OVERRIDE"})
add("_response_error_result",
    {"response": {}, "default_message": "fallback"}, io=None)
add("_response_error_result", {"response": None, "default_message": "fallback"}, io=None)
add("_response_error_result",
    {"response": {"code": None, "msg": None}, "default_message": "d"}, io=None)
add("_response_error_result",
    {"response": {"code": "X", "msg": "why"}, "default_message": "d", "override_error": ""})
add("_response_error_result",
    {"response": {"code": 7, "msg": "seven"}, "default_message": "d"}, io=None)
add("_response_error_result",
    {"response": {"code": True, "msg": False}, "default_message": "d"}, io=None)

# ── _finalize_send_result (3 possible seams) ──
add("_finalize_send_result",
    {"response": {"success": True, "data": {"message_id": "m1"}}, "default_message": "fail"},
    io={"_response_succeeded": [ret(True)], "_extract_response_field": [ret("m1")]})
add("_finalize_send_result",
    {"response": {"success": False, "code": 9, "msg": "nope"}, "default_message": "fail"},
    io={"_response_succeeded": [ret(False)],
        "_response_error_result": [ret({"success": False, "error": "[9] nope"})]})
add("_finalize_send_result",
    {"response": None, "default_message": "d"},
    io={"_response_succeeded": [ret(True)], "_extract_response_field": [ret(None)]})
add("_finalize_send_result",
    {"response": {"success": True}, "default_message": "d"},
    io={"_response_succeeded": [ret(True)], "_extract_response_field": [ret("X-1")]})
add("_finalize_send_result",
    {"response": {"success": True}, "default_message": "d"},
    io={"_response_succeeded": [ret(True)],
        "_extract_response_field": [rais("ValueError", "extract failed")]})
add("_finalize_send_result",
    {"response": {"success": False}, "default_message": "d"},
    io={"_response_succeeded": [ret(False)],
        "_response_error_result": [rais("RuntimeError", "err failed")]})
add("_finalize_send_result",
    {"response": {"success": True}, "default_message": "d"},
    io={"_response_succeeded": [rais("SystemExit", "gate exit")]})
add("_finalize_send_result",
    {"response": {"success": True}, "default_message": "d"},
    io={"_response_succeeded": [ret(1)], "_extract_response_field": [ret(42)]})

# ── _resolve_outbound_file_routing (pure) ──
add("_resolve_outbound_file_routing", {"file_path": "voice.ogg", "requested_message_type": "audio"})
add("_resolve_outbound_file_routing", {"file_path": "v.OPUS", "requested_message_type": "audio"})
add("_resolve_outbound_file_routing", {"file_path": "voice.ogg", "requested_message_type": "file"})
add("_resolve_outbound_file_routing", {"file_path": "clip.mp4", "requested_message_type": "file"})
add("_resolve_outbound_file_routing", {"file_path": "dir/clip.MOV", "requested_message_type": "x"})
add("_resolve_outbound_file_routing", {"file_path": "report.pdf", "requested_message_type": "file"})
add("_resolve_outbound_file_routing", {"file_path": "sheet.xlsx", "requested_message_type": "file"})
add("_resolve_outbound_file_routing", {"file_path": "noext", "requested_message_type": "file"})
add("_resolve_outbound_file_routing", {"file_path": "doc.docx", "requested_message_type": "file"})
add("_resolve_outbound_file_routing", {"file_path": "a.b.avi", "requested_message_type": "audio"})


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
        import oracle_feishu_adapter_flow5 as o
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
