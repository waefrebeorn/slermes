#!/usr/bin/env python3
"""Deterministic case generator for the Telegram adapter FLOW part 12 oracle.

Writes tests/fixtures/telegram_adapter_flow12.jsonl. Each case is
{"func", "in": {"args", "state"?, "io"}}. Re-running reproduces byte-identical
fixtures. Every ported function gets >= 6 cases; every seam name appears in at
least one case's io (asserted at the bottom).

Fixture domains (documented so C need not model outside them):
- _compress_image_to_jpeg drives early returns (OSError size, jpeg ext, small
  file, non-raster, Pillow-missing, pil_work failure) and success; PIL internals
  are silent doubles, only the seven listed seams are observable.
- _chat_send_lock uses string task/lock markers; asyncio doubles carry no live
  identity beyond the scripted ids.
- _download_observed_media source/file_obj are attribute doubles with only the
  fields named in the recorder projections.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "telegram_adapter_flow12.jsonl"

SEAM_NAMES = [
    "_set_reaction", "task_exception", "logger_debug",
    "_callback_authorized", "answer_callback",
    "recovery_in_flight", "monotonic", "schedule_polling_recovery",
    "task_done",
    "create_forum_topic", "logger_info", "logger_warning", "redact",
    "delete_webhook", "await_with_thread_deadline", "looks_like_network_error",
    "observed_media_source", "int_or_zero", "local_root", "fetch_telegram_file",
    "cache_media_bytes_async",
    "get_general_request_drain_lock", "general_req_shutdown", "general_req_initialize",
    "bounded_request_step",
    "event_is_set", "event_set",
    "normalize_telegram_chat_id", "current_task", "new_lock", "lock_aenter",
    "lock_aexit", "lock_waiters",
    "getsize", "sniff_raster", "pil_import", "mkstemp", "pil_work",
    "getenv",
]

CASES = []
ALL_IO = set()
ALL_FUNCS = set()


def case(func, args=None, state=None, io=None):
    c = {"func": func, "in": {"args": args or {}}}
    if state is not None:
        c["in"]["state"] = state
    c["in"]["io"] = io if io is not None else {}
    return c


def sc(ret=None, raise_type=None, raise_msg=""):
    d = {"__script__": True}
    if raise_type is not None:
        d["raise"] = {"type": raise_type, "message": raise_msg}
    else:
        d["ret"] = ret
    return d


def add(c):
    ALL_IO.update((c["in"].get("io") or {}).keys())
    ALL_FUNCS.add(c["func"])
    CASES.append(c)


# ── _clear_reactions ────────────────────────────────────────────────────────
for cid, mid, res in [
    ("c1", "m1", True), ("c2", "m2", False), ("c3", "m3", None),
    ("c4", "m4", {"ok": 1}),
    ("c5", "m5", sc(raise_type="RuntimeError", raise_msg="boom")),
    ("c6", "m6", sc(raise_type="SystemExit", raise_msg="bye")),
    ("c7", "m7", sc(raise_type="KeyboardInterrupt", raise_msg="int")),
]:
    add(case("_clear_reactions", args={"chat_id": cid, "message_id": mid},
             io={"_set_reaction": res}))

# ── _clear_task_attrs_except ───────────────────────────────────────────────
for cur, attrs, state in [
    ({"id": "c1"}, ["a"], {"a": {"id": "c1"}, "b": {"id": "x"}}),
    ({"id": "c1"}, ["a", "b"], {"a": {"id": "x"}, "b": {"id": "c1"}}),
    ({"id": "c2"}, ["z"], {}),
    ({"id": "c3"}, ["a", "b", "c"], {"a": {"id": "c3"}, "b": {"id": "c3"}, "c": {"id": "no"}}),
    ({"id": "c4"}, [], {"a": {"id": "x"}}),
    ({"id": "c5"}, ["a"], {"a": None, "other": 7}),
    ({"id": "c6"}, ["a", "b"], {"a": {"id": "c6"}, "b": None}),
]:
    add(case("_clear_task_attrs_except", args={"current": cur, "attrs": attrs}, state=state))

# ── _consume_abandoned_task ────────────────────────────────────────────────
for exc, log in [
    ("ok", "unused"),
    (sc(raise_type="CancelledError", raise_msg="c"), "unused"),
    (sc(raise_type="RuntimeError", raise_msg="r"), "ok"),
    (sc(raise_type="ValueError", raise_msg="v"), "ok"),
    (sc(raise_type="SystemExit", raise_msg="bye"), "unused"),
    (sc(raise_type="KeyboardInterrupt", raise_msg="int"), "unused"),
    (sc(raise_type="OSError", raise_msg="o"), "ok"),
]:
    add(case("_consume_abandoned_task", args={"task": {"id": "t"}},
             io={"task_exception": exc, "logger_debug": log}))

# ── _claim_callback_state ──────────────────────────────────────────────────
for auth, state, key, pop, ans in [
    (True, {"k": "sess"}, "k", True, "unused"),
    (True, {}, "k", True, "ok"),
    (False, {"k": "sess"}, "k", True, "unused"),
    (True, {"k": "sess"}, "k", False, "unused"),
    (sc(raise_type="RuntimeError", raise_msg="boom"), {"k": "s"}, "k", True, "unused"),
    (True, {"k": None}, "k", True, "ok"),
    (True, {"k": "s"}, "k", True, sc(raise_type="SystemExit", raise_msg="bye")),
    (True, {"k": ""}, "k", True, "ok"),
]:
    add(case("_claim_callback_state",
             args={"query": {"id": "q"}, "cb": {"chat_id": 1}, "state": state,
                   "key": key, "denial": "denied", "resolved": "resolved", "pop": pop},
             io={"_callback_authorized": auth, "answer_callback": ans}))

# ── _check_polling_stall ───────────────────────────────────────────────────
# (state, recovery, now, sched)
for state, recovery, now, sched in [
    ({"_webhook_mode": True}, "unused", "unused", "unused"),
    ({"_teardown_started": True}, "unused", "unused", "unused"),
    ({}, True, "unused", "unused"),
    ({}, False, "unused", "unused"),
    ({"_polling_last_progress_monotonic": 100.0}, False, 200.0, "unused"),
    ({"_polling_last_progress_monotonic": 10.0, "_polling_generation": 7}, False, 210.0, "ok"),
    ({"_polling_generation_started_monotonic": 5.0}, False, 305.0, "ok"),
    ({"_polling_last_progress_monotonic": 1.0}, False, sc(raise_type="RuntimeError", raise_msg="boom"), "unused"),
]:
    full = {"_webhook_mode": False, "_teardown_started": False, "has_fatal_error": False}
    full.update(state)
    add(case("_check_polling_stall", state=full,
             io={"recovery_in_flight": recovery, "monotonic": now,
                 "schedule_polling_recovery": sched}))

# ── _collect_live_tasks ────────────────────────────────────────────────────
for cands, cur, done in [
    ([{"id": "a"}, {"id": "b"}], {"id": "cur"}, [False, True]),
    ([{"id": "a"}, {"id": "a"}], {"id": "cur"}, [False, False]),
    ([{"id": "a"}, {"id": "b"}], {"id": "b"}, [False, False]),
    ([None, {"id": "a"}], {"id": "cur"}, [False]),
    ([{"id": "a"}], {"id": "cur"}, [True]),
    ([{"id": "a"}], {"id": "cur"}, sc(raise_type="RuntimeError", raise_msg="boom")),
    ([], {"id": "cur"}, "unused"),
    ([{"id": "a"}, {"id": "b"}, {"id": "a"}], {"id": "a"}, [False, False, False]),
]:
    add(case("_collect_live_tasks", args={"candidates": cands, "current": cur},
             io={"task_done": done}))

# ── _create_dm_topic ───────────────────────────────────────────────────────
for bot_none, res, li, lw, rd in [
    (True, "unused", "unused", "unused", "unused"),
    (False, {"message_thread_id": 42}, "ok", "unused", "unused"),
    (False, {"message_thread_id": 99}, "ok", "unused", "unused"),
    (False, sc(raise_type="RuntimeError", raise_msg="topic_name_duplicate"), "unused", "unused", "unused"),
    (False, sc(raise_type="RuntimeError", raise_msg="already exists"), "unused", "unused", "unused"),
    (False, sc(raise_type="RuntimeError", raise_msg="not a forum"), "unused", "ok", "unused"),
    (False, sc(raise_type="RuntimeError", raise_msg="forums_disabled"), "unused", "ok", "unused"),
    (False, sc(raise_type="RuntimeError", raise_msg="secret boom"), "unused", "ok", "redacted"),
    (False, sc(raise_type="SystemExit", raise_msg="bye"), "unused", "unused", "unused"),
]:
    add(case("_create_dm_topic",
             args={"chat_id": 7, "name": "topic", "bot_none": bot_none},
             io={"create_forum_topic": res, "logger_info": li, "logger_warning": lw, "redact": rd}))

# ── _delete_webhook_best_effort ────────────────────────────────────────────
for bot_kind, dw, aw, net, rd, lw, req in [
    ("none", "unused", "unused", "unused", "unused", "unused", False),
    ("no_method", "unused", "unused", "unused", "unused", "unused", False),
    ("ok", "ok", "ok", "unused", "unused", "unused", False),
    ("ok", "ok", sc(raise_type="OSError", raise_msg="net down"), True, "redacted", "ok", False),
    ("ok", "ok", sc(raise_type="OSError", raise_msg="net down"), True, "unused", "unused", True),
    ("ok", "ok", sc(raise_type="RuntimeError", raise_msg="boom"), False, "unused", "unused", False),
    ("ok", "ok", sc(raise_type="SystemExit", raise_msg="bye"), "unused", "unused", "unused", False),
    ("ok", "ok", sc(raise_type="OSError", raise_msg="x"), sc(raise_type="RuntimeError", raise_msg="netcheck"), "unused", "unused", False),
    ("ok", "ok", sc(raise_type="OSError", raise_msg="x"), True, sc(raise_type="RuntimeError", raise_msg="red"), "unused", False),
]:
    add(case("_delete_webhook_best_effort", args={"bot_kind": bot_kind, "require_success": req},
             io={"delete_webhook": dw, "await_with_thread_deadline": aw,
                 "looks_like_network_error": net, "redact": rd, "logger_warning": lw}))

# ── _download_observed_media ───────────────────────────────────────────────
# source tuple = [source_obj, filename, mime, kind]; fetch = [file_obj, data]
SRC = {"file_size": 100}
FO = {"file_path": "/a/b/photo.png"}
for src, ioz, lr, fetch, cache, lw, rd in [
    ([None, None, None, None], "unused", "unused", "unused", "unused", "unused", "unused"),
    ([{"file_size": 0}, None, None, None], 0, "unused", "unused", "unused", "unused", "unused"),
    ([{"file_size": 999}, None, None, None], 999, "unused", "unused", "unused", "unused", "unused"),
    ([SRC, "p.png", "image/png", "photo"], 100, "root", [FO, "DATA"], "CACHED", "unused", "unused"),
    ([SRC, None, "image/png", "photo"], 100, "root", [FO, "DATA"], "CACHED", "unused", "unused"),
    ([SRC, "p.png", "image/png", "photo"], 100, "root", sc(raise_type="OSError", raise_msg="x"), "unused", "ok", "redacted"),
    ([SRC, "p.png", "image/png", "photo"], 100, "root", [FO, "DATA"], None, "unused", "unused"),
    ([SRC, "p.png", "image/png", "photo"], 100, "root", [FO, "DATA"], sc(raise_type="RuntimeError", raise_msg="c"), "ok", "redacted"),
    ([SRC, "p.png", "image/png", "photo"], 100, "root", sc(raise_type="SystemExit", raise_msg="bye"), "unused", "unused", "unused"),
]:
    add(case("_download_observed_media", args={"msg": {"id": 1}, "what": "attachment"},
             state={"_max_doc_bytes": 500},
             io={"observed_media_source": sc(ret=src), "int_or_zero": ioz, "local_root": lr,
                 "fetch_telegram_file": fetch if (isinstance(fetch, dict) and "__script__" in fetch) else sc(ret=fetch) if isinstance(fetch, list) else fetch,
                 "cache_media_bytes_async": cache, "logger_warning": lw, "redact": rd}))

# ── _drain_general_connections_after_pool_timeout ──────────────────────────
for bot_kind, glock, shut, init, step, lw in [
    ("none", "unused", "unused", "unused", "unused", "unused"),
    ("bad_request", "unused", "unused", "unused", "unused", "unused"),
    ("ok", "lock", "stop", "start", [False, False], "unused"),
    ("ok", "lock", "stop", "start", [False, True], "ok"),
    ("ok", "lock", sc(raise_type="RuntimeError", raise_msg="s"), "unused", "unused", "unused"),
    ("ok", "lock", "stop", sc(raise_type="RuntimeError", raise_msg="i"), "unused", "unused"),
    ("ok", "lock", "stop", "start", sc(raise_type="SystemExit", raise_msg="bye"), "unused"),
    ("ok", sc(raise_type="RuntimeError", raise_msg="lock"), "unused", "unused", "unused", "unused"),
]:
    add(case("_drain_general_connections_after_pool_timeout", args={"bot_kind": bot_kind},
             io={"get_general_request_drain_lock": glock, "general_req_shutdown": shut,
                 "general_req_initialize": init, "bounded_request_step": step, "logger_warning": lw}))

# ── _disarm_ptb_retry_loop ─────────────────────────────────────────────────
for app_kind, attrs, isset, setev, ldbg in [
    ("none", [], "unused", "unused", "unused"),
    ("no_updater", [], "unused", "unused", "unused"),
    ("updater", [], "unused", "unused", "ok"),
    ("updater", ["_Updater__polling_task_stop_event"], False, "ok", "ok"),
    ("updater", ["_Updater__polling_task_stop_event"], True, "unused", "unused"),
    ("updater", ["_polling_task_stop_event"], False, "ok", "ok"),
    ("updater", ["_Updater__polling_task_stop_event"], sc(raise_type="RuntimeError", raise_msg="x"), "unused", "unused"),
    ("updater", ["_Updater__polling_task_stop_event"], False, sc(raise_type="SystemExit", raise_msg="bye"), "unused"),
]:
    add(case("_disarm_ptb_retry_loop", args={"app_kind": app_kind, "attrs": attrs},
             io={"event_is_set": isset, "event_set": setev, "logger_debug": ldbg}))

# ── _chat_send_lock ────────────────────────────────────────────────────────
FIRST = "_Updater__polling_task_stop_event"
for norm, cur, nlock, ent, exit_, waiters, state in [
    ("c1", {"id": "t1"}, "lock", "ok", "ok", [], {}),
    ("c1", {"id": "t1"}, "lock", "ok", "ok", [], {"_telegram_chat_send_locks": {"c1": {"id": "lock"}},
                                                   "_telegram_chat_send_lock_owners": {"c1": {"id": "t1"}}}),
    ("c2", {"id": "t2"}, "lock", "ok", "ok", [1], {}),
    ("c3", {"id": "t3"}, "lock", "ok", "ok", [], {}),
    ("c4", {"id": "t4"}, sc(raise_type="RuntimeError", raise_msg="n"), "unused", "unused", [], {}),
    ("c5", sc(raise_type="RuntimeError", raise_msg="c"), "unused", "unused", "unused", [], {}),
    ("c6", {"id": "t6"}, "lock", sc(raise_type="SystemExit", raise_msg="e"), "unused", [], {}),
    ("c7", {"id": "t7"}, "lock", "ok", "ok", sc(raise_type="RuntimeError", raise_msg="w"), {}),
]:
    io = {"normalize_telegram_chat_id": norm, "current_task": cur, "new_lock": nlock,
          "lock_aenter": ent, "lock_aexit": exit_,
          "lock_waiters": waiters}
    if isinstance(waiters, list) and waiters and isinstance(waiters[0], int):
        io["lock_waiters"] = True
    elif isinstance(waiters, list):
        io["lock_waiters"] = False
    add(case("_chat_send_lock", args={"chat_id": "raw"}, state=state, io=io))

# ── _compress_image_to_jpeg ────────────────────────────────────────────────
IMG = {"mode": "RGB", "size": [10, 10]}
for path, gsz, sniff, pil, mk, work, li, lw in [
    ("/a/x.png", sc(raise_type="OSError", raise_msg="gone"), "unused", "unused", "unused", "unused", "unused", "unused"),
    ("/a/x.JPG", 2000000, "unused", "unused", "unused", "unused", "unused", "unused"),
    ("/a/small.png", 1000, "unused", "unused", "unused", "unused", "unused", "unused"),
    ("/a/gif.gif", 2000000, None, "unused", "unused", "unused", "unused", "unused"),
    ("/a/p.png", 2000000, "png", sc(raise_type="ImportError", raise_msg="no PIL"), "unused", "unused", "unused", "ok"),
    ("/a/p.png", 2000000, "png", "PIL", "tmp1.jpg", sc(raise_type="ValueError", raise_msg="bad"), "unused", "ok"),
    ("/a/p.png", 2097152, "png", "PIL", "tmp2.jpg", "ok", "ok", "unused"),
    ("/a/p.png", 1048576, "png", "PIL", "tmp3.jpg", "ok", "ok", "unused"),
    ("/a/p.png", 2000000, "png", "PIL", sc(raise_type="OSError", raise_msg="mk"), "unused", "unused", "ok"),
]:
    add(case("_compress_image_to_jpeg",
             args={"image_path": path, "name": "bot", "image_spec": IMG},
             io={"getsize": gsz, "sniff_raster": sniff, "pil_import": pil, "mkstemp": mk,
                 "pil_work": work, "logger_info": li, "logger_warning": lw}))

# ── pure helpers ───────────────────────────────────────────────────────────
for ct, forum in [("private", False), ("supergroup", True), ("supergroup", False),
                  ("group", False), (None, False), ("", False), (5, False), ("  CHANNEL ", False)]:
    add(case("_normalize_chat_type", args={"chat_type": ct, "is_forum": forum}))

for name, default, val, mn, mx in [
    ("V", 1.0, "2.5", None, None),
    ("V", 1.0, "abc", None, None),
    ("V", 1.0, "nan", None, None),
    ("V", 1.0, "inf", None, None),
    ("V", 0.0, "-5", 0.0, None),
    ("V", 0.0, "100", None, 10.0),
    ("V", 3.0, None, None, None),
]:
    add(case("_env_float_clamped",
             args={"name": name, "default": default, "min_value": mn, "max_value": mx},
             io={"getenv": val}))

for mode, meta in [
    ("important", None), ("off", None), ("important", {"notify": True}),
    ("important", {"is_approval_prompt": True}), ("important", {}), (None, {"notify": False}),
]:
    add(case("_notification_kwargs", state={"_notifications_mode": mode}, args={"metadata": meta}))

for val in ["x", 3, True, 1.5, None, [], False]:
    add(case("_is_id_like", args={"value": val}))

for key, default, extra in [
    ("k", False, {"k": "YES"}), ("k", False, {"k": "off"}), ("k", True, {"k": "weird"}),
    ("k", False, {"k": 0}), ("k", False, {"k": 1}), ("k", True, {}), ("k", False, None),
]:
    add(case("_coerce_bool_extra", args={"key": key, "default": default, "extra": extra}))


# ── assertions ──
counts = {}
for c in CASES:
    counts[c["func"]] = counts.get(c["func"], 0) + 1
FUNCS = set(counts)
assert all(v >= 6 for v in counts.values()), {k: v for k, v in counts.items() if v < 6}
assert ALL_IO == set(SEAM_NAMES), ("seam coverage drift", set(SEAM_NAMES) - ALL_IO, ALL_IO - set(SEAM_NAMES))

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", encoding="utf-8", newline="\n") as f:
    for c in CASES:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")
print(f"wrote {len(CASES)} cases to {OUT} ({len(FUNCS)} funcs, {len(ALL_IO)} seams)")
for f in sorted(counts):
    print(f"  {f}: {counts[f]}")
