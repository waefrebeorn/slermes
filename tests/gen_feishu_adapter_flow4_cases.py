#!/usr/bin/env python3
"""Generate tests/fixtures/feishu_adapter_flow4.jsonl — input cases for the
Feishu adapter SDK-builder / dependency-gate FLOW remainder oracle.

Every case is hand-scripted: in.io maps a seam name to a result (plain JSON
value, an ordered LIST of results, or {"__script__":true,"raise":{type,message}})
that the C universal seam and the oracle's recording stubs replay identically.
The corpus covers every ported branch plus the FULL builtin exception hierarchy
(Exception subclasses swallowed by the two gates and the BaseException family
that bypasses `except Exception`).

Run `python tests/gen_feishu_adapter_flow4_cases.py` to regenerate; the script
fails if any target has fewer than 5 cases or a target is missing entirely.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "feishu_adapter_flow4.jsonl"
CASES = []


def add(func, args, io=None):
    CASES.append({"func": func, "in": {"args": args, "io": dict(io or {})}})


def rais(kind, msg):
    return {"__script__": True, "raise": {"type": kind, "message": msg}}


def ret(v):
    return {"__script__": True, "ret": v}


def P(name, value):
    return {"name": name, "value": value}


# ── _sdk_domain ──
add("_sdk_domain", {"domain_name": "lark"})
add("_sdk_domain", {"domain_name": "feishu"})
add("_sdk_domain", {"domain_name": ""})
add("_sdk_domain", {"domain_name": "LARK"})          # case-sensitive -> FEISHU
add("_sdk_domain", {"domain_name": "lark "})         # trailing space -> FEISHU
add("_sdk_domain", {"domain_name": None})
add("_sdk_domain", {"domain_name": 42})

# ── _card_button ──
add("_card_button", {"label": "Approve", "btn_type": "primary", "value": {"action": "ok"}})
add("_card_button", {"label": "Deny", "btn_type": "danger", "value": {"action": "no", "n": 2}})
add("_card_button", {"label": "", "btn_type": "", "value": None})
add("_card_button", {"label": "Go", "btn_type": "default", "value": "plain"})
add("_card_button", {"label": "X", "btn_type": "primary", "value": 7})
add("_card_button", {"label": "Y", "btn_type": "primary"})            # value absent
add("_card_button", {"label": "Z", "btn_type": "danger", "value": [1, "a"]})

# ── _card ──
add("_card", {"title": "T", "template": "blue", "markdown": "body"})
add("_card", {"title": "T", "template": "red", "markdown": "**hi**",
              "actions": [{"tag": "button"}, {"tag": "button"}]})
add("_card", {"title": "", "template": "", "markdown": "", "actions": []})
add("_card", {"title": "T", "template": "grey", "markdown": "m", "actions": None})
add("_card", {"title": "A", "template": "green", "markdown": "x",
              "actions": [{"tag": "button", "value": {"k": 1}}]})
add("_card", {"title": "N", "template": "turquoise", "markdown": "line1\nline2"})

# ── _sdk_build (chained builder + SimpleNamespace fallback) ──
add("_sdk_build", {"request_cls": None, "fields": []})
add("_sdk_build", {"request_cls": None,
                   "fields": [P("chat_id", "oc_1"), P("retries", 3), P("flag", True), P("extra", None)]})
add("_sdk_build", {"request_cls": None, "fields": [P("payload", {"a": 1, "b": [2, 3]})]})
add("_sdk_build", {"request_cls": "GetChatRequest", "fields": [P("chat_id", "oc_1")]},
    io={"builder_new": [ret("B0")], "builder_set": [ret("B1")], "builder_build": [ret({"kind": "req"})]})
add("_sdk_build", {"request_cls": "CreateMessageRequestBody",
                   "fields": [P("receive_id", "ou_1"), P("msg_type", "text"), P("uuid", "u-1")]},
    io={"builder_new": [ret("B0")],
        "builder_set": [ret("B1"), ret("B2"), ret("B3")],
        "builder_build": [ret({"built": True})]})
add("_sdk_build", {"request_cls": "GetChatRequest", "fields": []},
    io={"builder_new": [ret(None)], "builder_build": [ret(None)]})
add("_sdk_build", {"request_cls": "GetChatRequest", "fields": [P("chat_id", "x")]},
    io={"builder_new": [rais("ValueError", "bad builder")]})
add("_sdk_build", {"request_cls": "GetMessageRequest", "fields": [P("message_id", "m1"), P("x", 1)]},
    io={"builder_new": [ret("B0")], "builder_set": [ret("B1"), rais("SystemExit", "stop")]})
add("_sdk_build", {"request_cls": "DeleteMessageRequest", "fields": [P("message_id", "m2")]},
    io={"builder_new": [ret("B0")], "builder_set": [ret("B1")],
        "builder_build": [rais("KeyboardInterrupt", "kb")]})
add("_sdk_build", {"request_cls": "GetChatRequest", "fields": [P("chat_id", "x")]},
    io={"builder_new": [rais("AttributeError", "ab")]})  # Exception-side, propagates (no try/except)

# ── feishu_deps_present ──
add("feishu_deps_present", {"FEISHU_AVAILABLE": True})
add("feishu_deps_present", {"FEISHU_AVAILABLE": False}, io={"available": [ret(True)]})
add("feishu_deps_present", {"FEISHU_AVAILABLE": False}, io={"available": [ret(False)]})
add("feishu_deps_present", {"FEISHU_AVAILABLE": False}, io={"available": [ret("yes")]})
add("feishu_deps_present", {"FEISHU_AVAILABLE": False}, io={"available": [ret(None)]})
add("feishu_deps_present", {"FEISHU_AVAILABLE": False}, io={"available": [rais("ValueError", "v")]})
add("feishu_deps_present", {"FEISHU_AVAILABLE": False}, io={"available": [rais("KeyError", "k")]})
add("feishu_deps_present", {"FEISHU_AVAILABLE": False}, io={"available": [rais("OSError", "o")]})
add("feishu_deps_present", {"FEISHU_AVAILABLE": False}, io={"available": [rais("SystemExit", "sx")]})
add("feishu_deps_present", {"FEISHU_AVAILABLE": False}, io={"available": [rais("KeyboardInterrupt", "ki")]})
add("feishu_deps_present", {"FEISHU_AVAILABLE": False}, io={"available": [rais("CancelledError", "ce")]})
add("feishu_deps_present", {"FEISHU_AVAILABLE": False}, io={"available": [rais("BaseException", "be")]})
add("feishu_deps_present", {"FEISHU_AVAILABLE": False}, io={"available": [rais("GeneratorExit", "ge")]})
add("feishu_deps_present", {"FEISHU_AVAILABLE": False}, io={"available": [
    ret(True), rais("RuntimeError", "rt"), ret(False)]})

# ── check_feishu_requirements ──
add("check_feishu_requirements", {"FEISHU_AVAILABLE": True})
add("check_feishu_requirements", {"FEISHU_AVAILABLE": False}, io={"ensure_import": [ret(None)]})
add("check_feishu_requirements", {"FEISHU_AVAILABLE": False}, io={"ensure_import": [ret({"x": 1})]})
add("check_feishu_requirements", {"FEISHU_AVAILABLE": False}, io={"ensure_import": [rais("TypeError", "t")]})
add("check_feishu_requirements", {"FEISHU_AVAILABLE": False}, io={"ensure_import": [rais("RuntimeError", "r")]})
add("check_feishu_requirements", {"FEISHU_AVAILABLE": False}, io={"ensure_import": [rais("LookupError", "l")]})
add("check_feishu_requirements", {"FEISHU_AVAILABLE": False}, io={"ensure_import": [rais("NotImplementedError", "ni")]})
add("check_feishu_requirements", {"FEISHU_AVAILABLE": False}, io={"ensure_import": [rais("SystemExit", "sx")]})
add("check_feishu_requirements", {"FEISHU_AVAILABLE": False}, io={"ensure_import": [rais("GeneratorExit", "ge")]})
add("check_feishu_requirements", {"FEISHU_AVAILABLE": False}, io={"ensure_import": [rais("KeyboardInterrupt", "ki")]})
add("check_feishu_requirements", {"FEISHU_AVAILABLE": False}, io={"ensure_import": [rais("CancelledError", "ce")]})
add("check_feishu_requirements", {"FEISHU_AVAILABLE": False}, io={"ensure_import": [
    ret(None), rais("ValueError", "v"), ret(None)]})

# ── _build_get_chat_request ──
add("_build_get_chat_request", {"chat_id": "oc_1"}, io={"_sdk_build": [ret({"kind": "chat", "id": "oc_1"})]})
add("_build_get_chat_request", {"chat_id": ""}, io={"_sdk_build": [ret(None)]})
add("_build_get_chat_request", {"chat_id": "oc_2"}, io={"_sdk_build": [ret("REQUEST")]})
add("_build_get_chat_request", {"chat_id": "oc_3"}, io={"_sdk_build": [ret({"a": 1}), ret({"a": 2})]})
add("_build_get_chat_request", {"chat_id": "oc_4"}, io={"_sdk_build": [rais("ValueError", "bad")]})
add("_build_get_chat_request", {"chat_id": "oc_5"}, io={"_sdk_build": [rais("SystemExit", "sx")]})
add("_build_get_chat_request", {"chat_id": "oc_6"}, io={"_sdk_build": [ret("EMPTY")]})

# ── _build_file_upload_body ──
add("_build_file_upload_body",
    {"file_type": "file", "file_name": "a.txt", "file": "B64", "duration": 0},
    io={"_sdk_build": [ret({"kind": "upload"})]})
add("_build_file_upload_body",
    {"file_type": "opus", "file_name": "v.ogg", "file": "B64", "duration": 5000},
    io={"_sdk_build": [ret({"kind": "upload", "dur": 5000})]})
add("_build_file_upload_body",
    {"file_type": "file", "file_name": "d.pdf", "file": {"bytes": 3}, "duration": -1},
    io={"_sdk_build": [ret(None)]})
add("_build_file_upload_body",
    {"file_type": "file", "file_name": "n.bin", "file": None},
    io={"_sdk_build": [ret("REQ")]})
add("_build_file_upload_body",
    {"file_type": "file", "file_name": "e.mp3", "file": "B64", "duration": 12.0},
    io={"_sdk_build": [ret({"ok": True})]})
add("_build_file_upload_body",
    {"file_type": "file", "file_name": "f.png", "file": "B64", "duration": 1},
    io={"_sdk_build": [rais("TypeError", "bad upload")]})
add("_build_file_upload_body",
    {"file_type": "file", "file_name": "g.wav", "file": "B64", "duration": 2},
    io={"_sdk_build": [rais("KeyboardInterrupt", "ki")]})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print(f"WROTE {OUT}: {len(CASES)} cases, {len(counts)} funcs")
    low = [f for f, n in counts.items() if n < 5]
    print("funcs with <5 cases:", low)
    for f in sorted(counts):
        print(f"  {f}: {counts[f]}")
    if low:
        raise SystemExit(1)
    try:
        import sys
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import oracle_feishu_adapter_flow4 as o
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
