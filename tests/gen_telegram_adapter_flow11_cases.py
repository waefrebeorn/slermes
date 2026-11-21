#!/usr/bin/env python3
"""Deterministic case generator for the Telegram adapter FLOW part 11 oracle.

Writes tests/fixtures/telegram_adapter_flow11.jsonl. Each case is
{"func", "in": {"args", "state"?, "io"?}}. Re-running reproduces
byte-identical fixtures. Every ported function gets >= 6 cases and every seam
name appears in at least one case's io (asserted at the bottom).

Fixture-domain notes (documented so C need not model outside them):
- _apply_yaml_config: telegram_cfg values are scalars (str/int/bool/null/list
  of scalars); mention_patterns (json.dumps) is excluded.
- _cache_inbound_document: doc.file_size keeps the "too large" early-return
  path; image/video/generic-cache branches excluded.
- _cache_inbound_av: fixtures drive the size-not-allowed and exception paths.
- _build_ptb_requests: HERMES_TELEGRAM_DISABLE_FALLBACK_IPS is "1" so DNS
  discovery / fallback-IP transport are excluded.
- _build_message_event: all collaborators are scripted; chat/user/message are
  attribute doubles with only the fields named in the recorder projections.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "telegram_adapter_flow11.jsonl"

SEAM_NAMES = [
    # batch 1
    "cache_replied_media", "logger_info", "_is_callback_user_authorized",
    "answer_callback", "normalize_telegram_chat_id", "loop_time",
    "task_done", "task_cancel", "_await_disconnect_step",
    "_download_observed_media", "append_observed_note", "observed_media_note",
    "_attach_cached", "yaml_env_setter", "yaml_env_set",
    # batch 2
    "ensure_future", "wait", "_await_with_thread_deadline", "fut_done",
    "fut_cancel", "gather", "progress_is_set", "redact",
    "gateway_chat_type", "effective_message_thread_id", "adopt_created_topic_thread",
    "resolve_topic_binding", "build_source", "reply_context", "resolve_channel_prompt",
    "group_identity_prompt", "group_reply_expected", "expand_link_entities",
    "edit_revision", "MessageEvent",
    "getenv", "platform_ssl_context", "telegram_verify", "env_float_clamped",
    "logger_warning", "discover_fallback_ips", "proxy_target_hosts",
    "resolve_proxy_url", "build_ptb_requests", "instrument_polling_request",
    "telegram_media_size_allowed", "note_instead_of_media", "surface_media_cache_failure",
    "is_permanent_fatal", "salvage_buffered_inbound", "collect_live_tasks",
    "isfuture", "current_task", "clear_task_attrs_except",
]

CASES = []


def case(func, args=None, state=None, io=None, **kw):
    c = {"func": func, "in": {"args": args or {}}}
    if state is not None:
        c["in"]["state"] = state
    c["in"]["io"] = io if io is not None else {}
    c["in"].update(kw)
    return c


def sc(ret=None, raise_type=None, raise_msg=""):
    d = {"__script__": True}
    if raise_type is not None:
        d["raise"] = {"type": raise_type, "message": raise_msg}
    else:
        d["ret"] = ret
    return d


def wrap(v):
    """A bare JSON array in io is a per-call SEQUENCE; wrap single list VALUES."""
    return sc(ret=v) if isinstance(v, list) else v


ALL_IO = set()


def add(c):
    ALL_IO.update((c["in"].get("io") or {}).keys())
    CASES.append(c)


# ══════════════════ BATCH 1 ══════════════════

# ── _cache_replied_media ────────────────────────────────────────────────────
# seam cache_replied_media(self, msg, event) -> awaited; function returns None.
for msg, evt, res in [
    ({"id": 1}, {"text": "a"}, None),
    ({"id": 2}, {"text": "b"}, "ok"),
    (None, None, None),
    ({"id": 3}, {"text": "c"}, {"x": 1}),
    ({"id": 4}, {"text": "d"}, sc(raise_type="RuntimeError", raise_msg="boom")),
    ({"id": 5}, {"text": "e"}, sc(raise_type="SystemExit", raise_msg="bye")),
    ({"id": 6}, {"text": "f"}, sc(raise_type="KeyboardInterrupt", raise_msg="int")),
]:
    add(case("_cache_replied_media", args={"msg": msg, "event": evt},
             io={"cache_replied_media": res}))

# ── _cache_dm_topic_from_message ────────────────────────────────────────────
for topics, cid, tid, tname, log in [
    ({}, "c1", "5", "general", "ok"),
    ({}, "c2", 7, "alpha", "ok"),
    ({"c1:general": 3}, "c1", "5", "general", "unused"),
    ({}, "c3", "0", "x", "ok"),
    ({}, "c4", "-3", "y", "ok"),
    ({}, "c5", "abc", "z", "unused"),  # int() ValueError
    ({}, "c6", "9", "topic", sc(raise_type="KeyboardInterrupt", raise_msg="bye")),
]:
    add(case("_cache_dm_topic_from_message",
             args={"chat_id": cid, "thread_id": tid, "topic_name": tname},
             state={"_dm_topics": topics, "name": "bot"},
             io={"logger_info": log}))

# ── _callback_authorized ────────────────────────────────────────────────────
for uid, cb, auth, ans in [
    ("11", {"chat_id": 1, "chat_type": "private", "thread_id": None, "user_name": "u"}, True, "unused"),
    ("12", {"chat_id": 2, "chat_type": None, "thread_id": 5, "user_name": None}, False, "ok"),
    (5, {"chat_id": 3, "chat_type": "group", "thread_id": "7", "user_name": "n"}, False, "ok"),
    ("", {"chat_id": 4, "chat_type": "channel", "thread_id": 0, "user_name": ""}, True, "unused"),
    ("9", {"chat_id": 5, "chat_type": "private", "thread_id": None, "user_name": "u"},
     sc(raise_type="RuntimeError", raise_msg="boom"), "unused"),
    ("9", {"chat_id": 6, "chat_type": "private", "thread_id": None, "user_name": "u"},
     False, sc(raise_type="SystemExit", raise_msg="bye")),
    ("9", {"chat_id": 7, "chat_type": "private", "thread_id": None, "user_name": "u"},
     False, sc(raise_type="KeyboardInterrupt", raise_msg="int")),
]:
    add(case("_callback_authorized",
             args={"query": {"from_user": {"id": uid}}, "cb": cb, "denial_text": "denied"},
             io={"_is_callback_user_authorized": auth, "answer_callback": ans}))

# ── _chat_outbound_slot_remaining ───────────────────────────────────────────
# normalize -> key string; loop_time -> now; deadline from state slot dict.
for slot, norm, now in [
    ({}, "c1", 0.0),
    ({"c1": 10.5}, "c1", 3.0),
    ({"c1": 2.0}, "c1", 5.0),
    ({"c1": 7.25}, "c1", 1.25),
    ({"c1": 5.0}, "c1", sc(raise_type="RuntimeError", raise_msg="boom")),
    ({}, sc(raise_type="SystemExit", raise_msg="bye"), 0.0),
    ({"c1": 9.0}, "c1", sc(raise_type="KeyboardInterrupt", raise_msg="int")),
]:
    add(case("_chat_outbound_slot_remaining", args={"chat_id": "raw"},
             state={"_telegram_chat_outbound_slot_until": slot},
             io={"normalize_telegram_chat_id": norm, "loop_time": now}))

# ── _cancel_task_attr ───────────────────────────────────────────────────────
# state[attr] is a task double; task.done()/cancel() and _await_disconnect_step seams.
TASK = {"id": "t1"}
for attr, taskstate, done, cancel, aw in [
    ("_task", TASK, False, "ok", "ok"),
    ("_task", TASK, True, "unused", "unused"),
    ("_task", None, "unused", "unused", "unused"),
    ("_task", TASK, False, "ok", sc(raise_type="RuntimeError", raise_msg="boom")),
    ("_task", TASK, sc(raise_type="RuntimeError", raise_msg="done"), "unused", "unused"),
    ("_task", TASK, False, sc(raise_type="SystemExit", raise_msg="bye"), "unused"),
    ("_task", TASK, False, "ok", sc(raise_type="KeyboardInterrupt", raise_msg="int")),
]:
    st = {attr: taskstate} if taskstate is not None or attr == "_task" else {}
    add(case("_cancel_task_attr", args={"attr": attr, "label": "step"},
             state=st,
             io={"task_done": done, "task_cancel": cancel, "_await_disconnect_step": aw}))

# ── _cache_observed_media ───────────────────────────────────────────────────
DL = sc(ret=["oversized", "BIG"])
for status_cached, note, li, om, at in [
    (["oversized", 99], "noted", "ok", "unused", "unused"),
    (["unreadable", None], "noted2", "unused", "unused", "unused"),
    (["ok", {"path": "p", "media_type": "image/png"}], "unused", "unused", "note3", "ok"),
    (["oversized", "X"], sc(raise_type="RuntimeError", raise_msg="boom"), "unused", "unused", "unused"),
    (sc(raise_type="RuntimeError", raise_msg="dl"), "unused", "unused", "unused", "unused"),
    (["unreadable", None], sc(raise_type="SystemExit", raise_msg="bye"), "unused", "unused", "unused"),
    (["ok", {"path": "q"}], "unused", "unused", sc(raise_type="KeyboardInterrupt", raise_msg="int"), "unused"),
]:
    add(case("_cache_observed_media",
             args={"msg": {"id": 1}, "event": {"text": "orig", "media_urls": ["old"],
                                                "media_types": ["t"], "media_text_inlined": [False]}},
             state={"_max_doc_bytes": 20 * 1024 * 1024},
             io={"_download_observed_media": wrap(status_cached), "append_observed_note": note,
                 "logger_info": li, "observed_media_note": om, "_attach_cached": at}))

# ── pure staticmethods: _caption_1024, _entity_type, _ext_from_path ─────────
for cap in ["hello", "", None, "x" * 1100, "a" * 1024, "123", "é" * 600]:
    add(case("_caption_1024", args={"caption": cap}, io={}))

for ent in [{"type": "MessageEntityType.TEXT"}, {"type": "URL"}, {}, {"type": 5}, {"type": "BOLD"}, None]:
    add(case("_entity_type", args={"entity": ent}, io={}))

for fp, cands, default in [
    ("a.mp4", ["mp4", "webm"], "bin"),
    ("VIDEO.MOV", ["mov"], "x"),
    ("none.txt", ["mp4"], "def"),
    (None, ["mp4"], "d2"),
    ("", [], "d3"),
    ("archive.tar.gz", ["gz"], "d4"),
]:
    add(case("_ext_from_path", args={"file_path": fp, "candidates": cands, "default": default}, io={}))

# ── _apply_yaml_config ──────────────────────────────────────────────────────
def yc(**kw):
    return {"yaml_cfg": {}, "telegram_cfg": kw}

YSET = "setter"
for y, setter, envs in [
    (yc(require_mention=True, guest_mode="YES", reactions="on"), YSET, "ok"),
    (yc(free_response_chats=["a", "b"], allowed_chats=[1, 2]), YSET, "ok"),
    (yc(proxy_url="  http://p  ", reply_to_mode=False), YSET, "ok"),
    (yc(extra={"reply_to_mode": "off", "custom_k": "v", "allow_from": "x"}), YSET, "ok"),
    (yc(disable_topic_auto_rename=True, dm_topic_auto_transfer=False), YSET, "ok"),
    (yc(), YSET, "ok"),
    (yc(require_mention=None, guest_mode=False), sc(raise_type="KeyboardInterrupt", raise_msg="bye"), "unused"),
]:
    # envs is a per-call sequence of writes (or a raise spec); scripts must carry
    # at least one entry because yaml_env_set is called on every env write, but
    # case 6 fails at the setter itself so no writes happen.
    io = {"yaml_env_setter": setter, "yaml_env_set": "ok"}
    add(case("_apply_yaml_config", args={"yaml_cfg": y["yaml_cfg"], "telegram_cfg": y["telegram_cfg"]}, io=io))


# ══════════════════ BATCH 2 ══════════════════

# ── _await_cold_start_readiness ─────────────────────────────────────────────
# ensure_future x2 -> task doubles; wait -> marker; deadline seam drives try/except.
# finally: fut_done/fut_cancel per task, then gather. is_set sequence, redact.
for df, ens, waitv, deadline, dones, gathers, issets, red in [
    ("ok", ["p", "e"], "w", "unused", [True, True], "ok", [False, False], "unused"),
    ("ok", ["p", "e"], "w", sc(raise_type="TimeoutError", raise_msg="t"), [False, False], "ok", [False], "unused"),
    ("ok", ["p", "e"], "w", "unused", [True, True], "ok", [False, True], "unused"),
    ("ok", ["p", "e"], "w", "unused", [False, True], "ok", [False, False], "redacted"),
    ("ok", ["p", "e"], "w", sc(raise_type="RuntimeError", raise_msg="r"), [True, False], "ok", [False], "unused"),
    ("ok", ["p", "e"], "w", "unused", [True, True], "ok", [True], "unused"),
    (sc(raise_type="SystemExit", raise_msg="bye"), "unused", "unused", "unused", [], "unused", [], "unused"),
]:
    io = {"ensure_future": df,
          "wait": waitv, "_await_with_thread_deadline": deadline,
          "fut_done": dones,
          "fut_cancel": "ok", "gather": gathers,
          "progress_is_set": issets,
          "redact": red}
    add(case("_await_cold_start_readiness",
             args={"strict_error": [{"type": "RuntimeError", "message": "boom"}]} if red == "redacted" else {}, io=io))

# ── _build_message_event ────────────────────────────────────────────────────
ME = {
    "gateway_chat_type": "group", "effective_message_thread_id": "42",
    "adopt_created_topic_thread": "42", "resolve_topic_binding": ["topic", "skill"],
    "build_source": {"user_id": "11", "user_name": "Alice"},
    "reply_context": ["7", "quoted"], "resolve_channel_prompt": "prompt",
    "group_identity_prompt": "identity", "group_reply_expected": True,
    "expand_link_entities": "hello world", "edit_revision": 3,
}
for overrides, mtype, uid in [
    ({}, "TEXT", 99),
    ({"gateway_chat_type": "dm", "build_source": {"user_id": "11", "user_name": "A"}}, "TEXT", 0),
    ({"gateway_chat_type": "channel", "build_source": {"user_id": "11", "user_name": "C"}}, "PHOTO", 5),
    ({"resolve_topic_binding": [None, None], "reply_context": [None, None]}, "TEXT", None),
    ({"build_source": sc(raise_type="RuntimeError", raise_msg="boom")}, "TEXT", 1),
    ({"gateway_chat_type": sc(raise_type="SystemExit", raise_msg="bye")}, "TEXT", 1),
    ({"MessageEvent": sc(raise_type="KeyboardInterrupt", raise_msg="int")}, "TEXT", 1),
]:
    io = {}
    for k, v in ME.items():
        io[k] = wrap(v) if isinstance(v, list) else v
    io["MessageEvent"] = "EVENT"
    io.update({k: (wrap(v) if isinstance(v, list) else v) for k, v in overrides.items()})
    add(case("_build_message_event",
             args={"message": {"message_id": 100, "date": 1234.0,
                               "chat": {"id": 999, "title": "Chat", "full_name": "Full"},
                               "from_user": {"id": 11, "full_name": "Alice", "is_bot": False}},
                   "msg_type": mtype, "update_id": uid},
             state={"config": {"extra": {"gk": "gv"}}},
             io=io))

# ── _build_ptb_requests ─────────────────────────────────────────────────────
for getenv, ssl, verify, proxy, info, pair, inst in [
    ("1", "CTX", "V", "http://proxy", "ok", [["REQ", "GETUPD"]], "INST"),
    ("1", "CTX2", None, None, "ok", [["R", "G"]], "I2"),
    ("1", "C", "V", "http://p2", "ok", [[None, {"id": 2}]], "I3"),
    ("1", "C", "V", "http://p3", sc(raise_type="RuntimeError", raise_msg="boom"), "u", "u"),
    ("1", "C", "V", "http://p4", "ok", sc(raise_type="RuntimeError", raise_msg="boom"), "u"),
    ("1", "C", "V", "http://p5", "ok", [["R", "G"]], sc(raise_type="SystemExit", raise_msg="bye")),
    ("1", "C", sc(raise_type="KeyboardInterrupt", raise_msg="int"), "u", "u", "u", "u"),
]:
    add(case("_build_ptb_requests", args={},
             state={"config": {"extra": {"base_url": "https://api", "proxy_url": "cfg"}}, "name": "bot"},
             io={"getenv": getenv, "platform_ssl_context": ssl, "telegram_verify": verify,
                 "env_float_clamped": 5.0, "logger_warning": "unused",
                 "discover_fallback_ips": "unused", "proxy_target_hosts": "unused",
                 "resolve_proxy_url": proxy, "logger_info": info,
                 "build_ptb_requests": pair, "instrument_polling_request": inst}))

# ── _cache_inbound_av ───────────────────────────────────────────────────────
for allowed, note, li, warn, surf in [
    ([False, "too big"], "ok", "ok", "unused", "unused"),
    ([False, None], "ok", "ok", "unused", "unused"),
    ([False, "no size"], "ok", "ok", "unused", "unused"),
    (sc(raise_type="RuntimeError", raise_msg="boom"), "unused", "unused", "ok", "ok"),
    (sc(raise_type="ValueError", raise_msg="v"), "unused", "unused", "ok", "ok"),
    ([False, "x"], sc(raise_type="SystemExit", raise_msg="bye"), "unused", "unused", "unused"),
    (sc(raise_type="SystemExit", raise_msg="bye"), "unused", "unused", "unused", "unused"),
]:
    add(case("_cache_inbound_av",
             args={"msg": {"id": 1}, "event": {"text": "e", "media_urls": [], "media_types": []},
                   "source": {"file_name": "a.mp4", "file_size": 999}, "label": "LBL",
                   "kind": "video", "ext": "mp4", "mime": "video/mp4"},
             io={"telegram_media_size_allowed": wrap(allowed), "note_instead_of_media": note,
                 "logger_info": li, "logger_warning": warn, "redact": "R",
                 "surface_media_cache_failure": surf}))

# ── _cache_inbound_document ─────────────────────────────────────────────────
for filesize, mime, fname, note, li, red, warn, surf in [
    (999999999, "image/png", "a.png", "ok", "ok", "unused", "unused", "unused"),
    (0, "image/png", "b.png", "ok", "ok", "unused", "unused", "unused"),
    (None, "text/plain", None, "ok", "ok", "unused", "unused", "unused"),
    (500000000, "video/mp4", "v.mp4", "ok", "ok", "unused", "unused", "unused"),
    (999999999, "image/png", "a.png", sc(raise_type="RuntimeError", raise_msg="boom"), "unused", "unused", "unused", "unused"),
    (999999999, "image/png", "a.png", "ok", sc(raise_type="RuntimeError", raise_msg="b"), "unused", "unused", "unused"),
    (999999999, "image/png", "a.png", sc(raise_type="RuntimeError", raise_msg="boom"), "ok", "R", "ok", sc(raise_type="SystemExit", raise_msg="bye")),
]:
    add(case("_cache_inbound_document",
             args={"msg": {"document": {"file_name": fname, "mime_type": mime, "file_size": filesize}},
                   "event": {"text": "cap", "media_urls": [], "media_types": []}},
             state={"_max_doc_bytes": 20 * 1024 * 1024},
             io={"logger_info": li, "note_instead_of_media": note, "redact": red,
                 "logger_warning": warn, "surface_media_cache_failure": surf}))

# ── _cancel_pending_delivery_tasks ──────────────────────────────────────────
for fatal, pending, kinds, gathers, warn, salv, clear in [
    (False, ["t1", "t2"], [True, True], "ok", "unused", "ok", "ok"),
    (True, ["t1", "t2"], [True, True], "ok", "ok", "unused", "ok"),
    (False, [], [], "unused", "unused", "ok", "ok"),
    (False, ["t1", "t2"], [False, False], "unused", "unused", "ok", "ok"),
    (False, ["t1"], [True], sc(raise_type="RuntimeError", raise_msg="boom"), "unused", "unused", "unused"),
    ("__RAISE_CT__", "unused", [], "unused", "unused", "unused", "unused"),
    (False, ["t1", "t2"], [True, True], "ok", "ok", "unused", sc(raise_type="KeyboardInterrupt", raise_msg="int")),
]:
    ct = sc(raise_type="SystemExit", raise_msg="bye") if fatal == "__RAISE_CT__" else "CUR"
    io = {"current_task": ct, "collect_live_tasks": wrap(pending) if isinstance(pending, list) else pending,
          "isfuture": wrap(kinds) if isinstance(kinds, list) else kinds,
          "task_cancel": "ok", "gather": gathers, "is_permanent_fatal": fatal,
          "logger_warning": warn, "salvage_buffered_inbound": salv,
          "clear_task_attrs_except": clear}
    st = {"_media_group_tasks": {}, "_media_group_events": {},
          "_pending_photo_batch_tasks": {}, "_pending_photo_batches": {},
          "_pending_text_batch_tasks": {}, "_pending_text_batches": {}}
    if fatal is True:
        st["_pending_text_batches"] = {"a": 1, "b": 2}
    add(case("_cancel_pending_delivery_tasks", args={}, state=st, io=io))


# ══════════════════ invariants ══════════════════
counts = {}
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
assert all(v >= 6 for v in counts.values()), counts
missing = sorted(set(SEAM_NAMES) - ALL_IO)
extra = sorted(ALL_IO - set(SEAM_NAMES))
assert not missing and not extra, (missing, extra)

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
print(f"wrote {len(CASES)} cases over {len(counts)} funcs to {OUT}")
for fn in sorted(counts):
    print(f"  {fn}: {counts[fn]}")
