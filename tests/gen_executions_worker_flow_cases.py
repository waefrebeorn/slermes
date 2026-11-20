#!/usr/bin/env python3
"""Generate JSONL fixtures for tests/oracle_executions_worker_flow.py.

Each case is {"func": <name>, "in": {"args": {...}, "io": {...}}} where `io`
scripts the two universal seams `_persist` and `_value` (the ONLY I/O in the
port). `args` holds the live method's parameters (minus self); for the module
helpers it holds their named parameters directly.

Shape rules (shared with the C port header):
  * io[name] = a plain value (every call) | a list (per-call sequence) |
    {"__raise__": cls, "message": m} (raised exception).
  * an enum `source` is a plain string or {"__enum__": value}.
  * identity fields (epoch/execution_id/session_id/generation/sequence) are
    simple strings/numbers; payloads are JSON objects/scalars.
  * a stat keyword dict is a JSON object; int()/str() seam results stay in the
    documented domain (numbers, bools, digit strings, null).

Run: python tests/gen_executions_worker_flow_cases.py
Writes tests/fixtures/executions_worker_flow.jsonl (>=6 cases per func).
"""
import json
import os
from pathlib import Path

HERE = Path(__file__).resolve()
OUT = HERE.parent / "fixtures" / "executions_worker_flow.jsonl"

CASES = []


def case(func, args, io=None):
    CASES.append({"func": func, "in": {"args": args, "io": io or {}}})


def ident(seed=0):
    return {
        "epoch": f"ep{seed}",
        "execution_id": f"ex{seed}",
        "session_id": f"ses{seed}",
        "generation": seed,
        "sequence": seed + 1,
    }


def with_ident(seed, **kw):
    d = ident(seed)
    d.update(kw)
    return d


def raised(cls, message):
    return {"__raise__": cls, "message": message}


# ── _key_set ──────────────────────────────────────────────────────────────
for keys, allowed, required in [
    ({"a": 1}, ["a", "b"], []),
    ({}, ["a", "b"], []),
    ({"a": 1, "b": 2}, ["a", "b"], []),
    ({"a": 1}, ["a", "b"], ["a"]),
    ({"a": 1, "b": 2}, ["a", "b"], ["a", "b"]),
    ({"a": 1, "b": 2, "c": 3}, ["a", "b"], []),      # extra key -> raise
    ({"b": 2}, ["a", "b"], ["a"]),                    # missing required -> raise
    ({"x": 1}, [], []),                               # empty allowed -> raise
]:
    case("_key_set", {"keys": keys, "allowed": allowed, "required": required})

# ── _record ───────────────────────────────────────────────────────────────
for i, (kind, status, seq) in enumerate([
    ("cron", "registered", 0), ("child", "running", 7),
    ("compute", "unknown", 1), ("kanban", "terminal", 99),
    ("cron", "running", 3), ("child", "registered", 12),
    ("compute", "terminal", 0),
]):
    case("_record", {"public": {
        "execution_id": f"ex{i}", "session_id": f"ses{i}", "kind": kind,
        "owner_epoch": f"ep{i}", "generation": i, "status": status,
        "last_sequence": seq,
    }})

# ── _messages ─────────────────────────────────────────────────────────────
for msgs in [
    [], [{"role": "user", "content": "hi"}],
    [{"role": "user", "content": "hi"}, {"role": "assistant", "content": "yo"}],
    ["plain", 5], [None, True, {"a": 1}],
    [{"a": 1}, "s", {"b": 2, "c": 3}],
]:
    case("_messages", {"messages": msgs})

# ── _annotations ──────────────────────────────────────────────────────────
AKEYS = ["_row_id", "timestamp", "_db_row_snapshot", "_canonical_row",
         "message_uid", "_tool_call_uids", "_tool_call_uid"]
for anns in [
    [], [{"_row_id": 1}],
    [{k: i for i, k in enumerate(AKEYS)}],
    [{"_row_id": 1, "message_uid": "m1"}, {"timestamp": 2.5}],
    [{"_canonical_row": {"x": 1}}],
    [{k: None for k in AKEYS}],
    [{"_tool_call_uids": ["a", "b"], "_row_id": None}],
]:
    case("_annotations", {"result": {"annotations": anns}})

# ── finish (_persist) ─────────────────────────────────────────────────────
for i, scr in enumerate([
    {"value": None}, {}, {"value": True, "error": None},
    raised("RuntimeError", "boom"),
    raised("AuthorityRefusal", "stale_epoch"),
    raised("SystemExit", "bye"),     # BaseException passthrough
    [{"value": 1}, {"value": 2}],    # per-call sequence
]):
    case("finish", with_ident(i), {"_persist": scr})

# ── record_usage (_value + key validation) ───────────────────────────────
for i, (usage, scr) in enumerate([
    ({"input_tokens": 10, "output_tokens": 5}, None),
    ({}, 0),
    ({"model": "m", "billing_provider": "p", "absolute": True}, 1),
    ({"bogus": 1}, None),                       # invalid key -> raise, no seam
    ({"input_tokens": 1}, raised("RuntimeError", "x")),
    ({"task": "t", "source": "s"}, "ok"),
    ({"estimated_cost_usd": 0.5}, 42),
]):
    io = {"_value": scr} if scr is not None else {}
    case("record_usage", with_ident(i, usage=usage), io)

# ── record_auxiliary_usage (_value + key validation) ─────────────────────
for i, (usage, scr) in enumerate([
    ({"input_tokens": 10}, None),
    ({}, 0),
    ({"model": "m", "billing_base_url": "u"}, 1),
    ({"output_tokens": 2, "api_call_count": 3}, 1),
    ({"bogus": 1}, None),
    ({"input_tokens": 1}, raised("AuthorityRefusal", "queue_full")),
    ({"estimated_cost_usd": None}, 7),
]):
    io = {"_value": scr} if scr is not None else {}
    case("record_auxiliary_usage", with_ident(i, usage=usage, task="aux-1"), io)

# ── acquire_turn_lease (_value + bool) ────────────────────────────────────
for i, scr in enumerate([True, False, 1, 0, "", "held", None, raised("RuntimeError", "busy")]):
    case("acquire_turn_lease", with_ident(i, lease={"holder": "h", "ttl_seconds": 300}),
         {"_value": scr})

# ── cleanup_turn_lease (_persist) ─────────────────────────────────────────
for i, scr in enumerate([{}, {"value": 1}, None, raised("RuntimeError", "x"),
                         raised("KeyboardInterrupt", "k"), [{"value": 0}, {"value": 1}]]):
    case("cleanup_turn_lease", with_ident(i, target_session_id="sesX", holder="h"),
         {"_persist": scr})

# ── set_session_title (_persist -> WorkerTitleResult) ────────────────────
for i, (src, scr) in enumerate([
    ("derived", {"value": True}),
    ("llm", {"value": False, "error": "taken", "title_candidate": "cand"}),
    ({"__enum__": "user"}, {"value": 1, "title_candidate": "t"}),
    (None, {"value": False}),
    ("user", raised("RuntimeError", "x")),
    ("derived", {"value": True, "error": None, "title_candidate": None}),
    ("llm", [{"value": False}, {"value": True}]),
]):
    case("set_session_title", with_ident(i, title="T", source=src), {"_persist": scr})

# ── set_session_title_source (_value + bool) ──────────────────────────────
for i, (src, scr) in enumerate([
    ("derived", True), ({"__enum__": "user"}, False), (None, 1),
    ("llm", 0), ("user", "yes"), ("derived", raised("RuntimeError", "x")),
    ({"__enum__": "llm"}, [True, False]),
]):
    case("set_session_title_source", with_ident(i, source=src), {"_value": scr})

# ── next_session_title (_value + str) ─────────────────────────────────────
for i, scr in enumerate(["Generated", "X", 42, True, None, raised("RuntimeError", "x"),
                         "  padded  ", "false"]):
    case("next_session_title", with_ident(i, base_title="base"), {"_value": scr})

# ── set_session_billing_route (_value) ────────────────────────────────────
for i, scr in enumerate([None, 0, 1, {"ok": 1}, raised("RuntimeError", "x"), [1, 2]]):
    case("set_session_billing_route",
         with_ident(i, route={"provider": "p", "base_url": "u", "billing_mode": None}),
         {"_value": scr})

# ── set_latest_user_api_content (_value + int) ────────────────────────────
for i, scr in enumerate([7, 0, "12", True, False, raised("RuntimeError", "x"), [5]]):
    case("set_latest_user_api_content",
         with_ident(i, content={"role": "user"}, api_content="ac"), {"_value": scr})

# ── set_message_api_content (_value + int) ────────────────────────────────
for i, scr in enumerate([3, 0, "9", 1, raised("ValueError", "bad"), None, [2, 4]]):
    case("set_message_api_content",
         with_ident(i, row_id=11, content="c", api_content="ac"), {"_value": scr})

# ── set_user_message_content (_value + conditional key + int) ─────────────
for i, (dm, scr) in enumerate([
    (None, 1), ({"theme": "dark"}, 2), ({}, 3),
    (None, raised("RuntimeError", "x")), ({"a": 1, "b": 2}, "4"),
    ({"a": 1}, [6]), (None, 0),
]):
    case("set_user_message_content",
         with_ident(i, row_id=5, content="c", display_metadata=dm), {"_value": scr})

# ── create_session (_value + key validation + required) ───────────────────
for i, (fields, scr) in enumerate([
    ({"source": "cli"}, "ses1"),
    ({"source": "cli", "model": "m", "user_id": "u"}, "ses2"),
    ({"source": "api", "cwd": "/a"}, "ses3"),
    ({"model": "m"}, None),                       # missing required -> raise
    ({"source": "cli", "bogus": 1}, None),        # extra key -> raise
    ({"source": "cli"}, raised("RuntimeError", "x")),
    ({"source": "cli", "session_key": "k", "chat_id": "c"}, "ses4"),
]):
    io = {"_value": scr} if scr is not None else {}
    case("create_session", with_ident(i, fields=fields), io)

# ── session_lifecycle (_value -> enum identity) ───────────────────────────
for i, scr in enumerate(["empty", "complete", "interrupted", "error",
                         raised("RuntimeError", "x"), ["empty", "complete"]]):
    case("session_lifecycle", with_ident(i), {"_value": scr})

# ── patch_session_sidecars (_value + Mapping check) ───────────────────────
for i, (patch, scr) in enumerate([
    ({"a": 1}, None), ({}, 0), ("raw", None), (None, None),
    ({"x": 1, "y": 2}, raised("RuntimeError", "x")), ([1, 2], None),
    ({"a": 1}, [1]),
]):
    case("patch_session_sidecars", with_ident(i, patch=patch), {"_value": scr})

# ── set_session_tool_pin (_value + Mapping check) ─────────────────────────
for i, (pin, scr) in enumerate([
    ({"tools": ["a"]}, None), ({}, 0), ("raw", None), (None, None),
    ({"x": 1}, raised("RuntimeError", "x")), ([1], None), ({"a": 1}, [1]),
]):
    case("set_session_tool_pin", with_ident(i, tool_pin=pin), {"_value": scr})

# ── append_transcript (_persist + key validation + messages + result) ─────
for i, (opts, scr) in enumerate([
    ({}, {"count": 1, "annotations": [{"_row_id": 1}]}),
    ({"turn_lease_holder": "h"}, {"count": 2, "annotations": []}),
    ({"bogus": 1}, None),
    ({}, {"count": 0, "annotations": [{"message_uid": "m"}, {"_row_id": 2}]}),
    ({"turn_lease_holder": None}, raised("RuntimeError", "x")),
    ({}, {"count": 3, "annotations": [{k: 1 for k in AKEYS}]}),
    ({"turn_lease_holder": "h2"}, [{"count": 1, "annotations": []},
                                   {"count": 2, "annotations": []}]),
]):
    io = {"_persist": scr} if scr is not None else {}
    case("append_transcript",
         with_ident(i, messages=[{"role": "user", "content": "hi"}, "raw"], options=opts), io)

# ── compression_watermark (_value + int) ──────────────────────────────────
for i, scr in enumerate([0, 5, "9", True, None, raised("RuntimeError", "x"), [7]]):
    case("compression_watermark", with_ident(i), {"_value": scr})

# ── compression_boundary (_value -> int|None) ─────────────────────────────
for i, scr in enumerate([0, 3, None, "7", raised("RuntimeError", "x"), [1, 2]]):
    case("compression_boundary",
         with_ident(i, peer={"session_key": "k", "source": "s"}), {"_value": scr})

# ── persist_operation (_persist + except RuntimeError translation) ────────
for i, (op, scr) in enumerate([
    ("execution.finish", {"value": 1}),
    ("compression.publish", raised("RuntimeError", "precondition text")),
    ("compression.publish", raised("RuntimeError", "another")),
    ("compression.publish", raised("NotImplementedError", "subclass")),
    ("compression.publish", raised("AuthorityRefusal", "typed")),
    ("execution.finish", raised("RuntimeError", "not publish")),
    ("compression.publish", raised("SystemExit", "base exc")),
    ("compression.publish", raised("KeyboardInterrupt", "base exc")),
    ("compression.publish", [raised("RuntimeError", "x"), {"value": 2}]),
]):
    case("persist_operation", with_ident(i, operation=op, payload={"p": 1}), {"_persist": scr})


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8", newline="\n") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False, separators=(",", ":")) + "\n")
    print(f"wrote {len(CASES)} cases to {OUT}")


if __name__ == "__main__":
    main()
