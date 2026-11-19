#!/usr/bin/env python3
"""Generate tests/fixtures/context_compressor_flow2.jsonl — the scripted-seam
case corpus for the context_compressor_flow2 FLOW + leaf port (24 funcs, >=5
cases each). ASCII scope; integral floats == ints. Covers normal paths, branch
edges, in-place state mutation, identity (is False) vs truthiness, real
scripted exceptions (incl. the BaseException trio), and fail-closed missing /
exhausted scripts. Run: python tests/gen_context_compressor_flow2_cases.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "context_compressor_flow2.jsonl")

END_MARKER = "--- END OF CONTEXT SUMMARY \u2014 respond to the message below, not the summary above ---"
MERGED_HEADER = "[PRIOR CONTEXT \u2014 for reference only; not a new message]"
NOTICE = "[Skills pruned during compression \u2014 reload before acting on these tasks]"
PLACEHOLDER = "[Old tool output cleared to save context space]"

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def msg(role, **kw):
    d = {"role": role}
    d.update(kw)
    return d


def cp():
    return {"type": "compaction"}


def raises(kind, message=""):
    return {"__raise__": kind, "message": message}


def attr(**kw):
    return {"__attr__": kw}


def callable_(seam):
    return {"__callable__": seam}


SESSION_DB = attr(get_active_message_watermark=callable_("session_db.get_active_message_watermark"),
                  get_message_role=callable_("session_db.get_message_role"))
PERSIST = attr(messages=attr(active_watermark=callable_("persistence.messages.active_watermark")))

# ── _is_checkpoint_item ──
add("_is_checkpoint_item", item=cp())
add("_is_checkpoint_item", item={"type": "other"})
add("_is_checkpoint_item", item={})
add("_is_checkpoint_item", item="compaction")
add("_is_checkpoint_item", item=None)
add("_is_checkpoint_item", item=attr(type="compaction"))
add("_is_checkpoint_item", item={"type": "compaction", "x": 1})

# ── _newest_checkpoint_carrier ──
add("_newest_checkpoint_carrier", messages=[msg("assistant", codex_reasoning_items=[cp()])])
add("_newest_checkpoint_carrier", messages=[msg("user", content="hi")])
add("_newest_checkpoint_carrier", messages=[
    msg("assistant", codex_reasoning_items=[cp()]),
    msg("user", content="x"),
    msg("assistant", codex_reasoning_items=[{"type": "other"}, cp()])])
add("_newest_checkpoint_carrier", messages=[msg("user", codex_reasoning_items=[cp()])])
add("_newest_checkpoint_carrier", messages=[msg("assistant", codex_reasoning_items="notlist")])
add("_newest_checkpoint_carrier", messages=[], key="codex_reasoning_items")
add("_newest_checkpoint_carrier", messages=[msg("assistant", other_key=[cp()])], key="other_key")

# ── _set_sidecar ──
add("_set_sidecar", state={"msg": msg("assistant", codex_reasoning_items=[cp()])},
    key="codex_reasoning_items", kept=[{"type": "other"}])
add("_set_sidecar", state={"msg": msg("assistant", codex_reasoning_items=[cp()])},
    key="codex_reasoning_items", kept=[])
add("_set_sidecar", state={"msg": msg("assistant")}, key="codex_reasoning_items", kept=[])
add("_set_sidecar", state={"msg": msg("assistant", codex_reasoning_items=[cp()])},
    key="codex_reasoning_items", kept=[{"type": "other"}, {"type": "x"}])
add("_set_sidecar", state={"msg": msg("assistant")}, key="k", kept=[1])

# ── drop_shadowed_checkpoints ──
add("drop_shadowed_checkpoints", state={"messages": [
    msg("assistant", codex_reasoning_items=[cp(), {"type": "other"}]),
    msg("user", content="x"),
    msg("assistant", codex_reasoning_items=[cp()])]}, key="codex_reasoning_items")
add("drop_shadowed_checkpoints", state={"messages": [
    msg("assistant", codex_reasoning_items=[cp()])]}, key="codex_reasoning_items")
add("drop_shadowed_checkpoints", state={"messages": [
    msg("assistant", codex_reasoning_items=[cp()]),
    msg("assistant", codex_reasoning_items=[cp()])]}, key="codex_reasoning_items", before=1)
add("drop_shadowed_checkpoints", state={"messages": [msg("user", codex_reasoning_items=[cp()])]},
    key="codex_reasoning_items")
add("drop_shadowed_checkpoints", state={"messages": [
    msg("assistant", codex_reasoning_items=[{"type": "other"}]),
    msg("assistant", codex_reasoning_items=[cp()])]}, key="codex_reasoning_items")
add("drop_shadowed_checkpoints", state={"messages": []}, key="codex_reasoning_items")

# ── _last_index_with_role ──
add("_last_index_with_role", messages=[msg("user"), msg("assistant"), msg("user")], role="user")
add("_last_index_with_role", messages=[msg("user"), msg("assistant")], role="assistant")
add("_last_index_with_role", messages=[msg("user")], role="tool")
add("_last_index_with_role", messages=["x", None, msg("user")], role="user")
add("_last_index_with_role", messages=[], role="user")
add("_last_index_with_role", messages=[msg("user"), "x"], role="user")

# ── _last_assistant_index ──
add("_last_assistant_index", messages=[msg("assistant"), msg("user")])
add("_last_assistant_index", messages=[msg("user"), msg("assistant"), msg("user")])
add("_last_assistant_index", messages=[msg("user")])
add("_last_assistant_index", messages=[])
add("_last_assistant_index", messages=[None, msg("assistant")])

# ── _prune_stale_reasoning_replay ──
add("_prune_stale_reasoning_replay", state={"messages": [msg("assistant", codex_reasoning_items=[cp()])]})
add("_prune_stale_reasoning_replay", state={"messages": [
    msg("assistant", codex_reasoning_items=[cp()]),
    msg("user", content="x"),
    msg("assistant", codex_reasoning_items=[cp()])]})
add("_prune_stale_reasoning_replay", state={"messages": [
    msg("assistant", codex_reasoning_items=[cp()]),
    msg("assistant", codex_reasoning_items=[cp()]),
    msg("user", content="x")]})
add("_prune_stale_reasoning_replay", state={"messages": [msg("user", content="x")]})
add("_prune_stale_reasoning_replay", state={"messages": []})

# ── stamp_db_persisted_markers ──
add("stamp_db_persisted_markers", state={"messages": [msg("user", content="x"), msg("assistant")]})
add("stamp_db_persisted_markers", state={"messages": [msg("user"), "x", None]})
add("stamp_db_persisted_markers", state={"messages": []})
add("stamp_db_persisted_markers", state={"messages": [msg("user", _db_persisted=False)]})
add("stamp_db_persisted_markers", state={"messages": [None, 5, msg("tool")]})

# ── _looks_like_compaction_summary ──
add("_looks_like_compaction_summary", msg=msg("assistant", _compressed_summary=True),
    content="body\n" + END_MARKER)
add("_looks_like_compaction_summary", msg=msg("assistant"),
    content=MERGED_HEADER + " body\n" + END_MARKER)
add("_looks_like_compaction_summary", msg=msg("tool", _compressed_summary=True),
    content="body\n" + END_MARKER)
add("_looks_like_compaction_summary", msg=msg("user"), content="body\n" + END_MARKER)
add("_looks_like_compaction_summary", msg=msg("assistant"),
    content="CONTEXT COMPACTION summary\n" + END_MARKER)
add("_looks_like_compaction_summary", msg=msg("assistant"), content="no marker here")
add("_looks_like_compaction_summary", msg=msg("assistant", _compressed_summary=True), content=None)
add("_looks_like_compaction_summary", msg=msg("system", _compressed_summary=True),
    content="Conversation Summary\n" + END_MARKER)

# ── _salvage_reduce_todo_snapshot ──
add("_salvage_reduce_todo_snapshot", state={"out": [
    msg("user", content="task"), msg("user", _todo_snapshot_synthetic=True, content="todo " + NOTICE + " rest")]})
add("_salvage_reduce_todo_snapshot", state={"out": [
    msg("user", content="task"), msg("user", _todo_snapshot_synthetic=True, content="just todo")]})
add("_salvage_reduce_todo_snapshot", state={"out": [msg("user", content="task")]})
add("_salvage_reduce_todo_snapshot", state={"out": [
    msg("assistant", _todo_snapshot_synthetic=True, content="x")]})
add("_salvage_reduce_todo_snapshot", state={"out": []})
add("_salvage_reduce_todo_snapshot", state={"out": [
    msg("user", _todo_snapshot_synthetic=True, content=NOTICE)]})

# ── salvage_grown_transcript ──
add("salvage_grown_transcript", original=[msg("user")], candidate=[], budget=None)
add("salvage_grown_transcript", original=[msg("user")], candidate=[msg("user")], budget=0)
add("salvage_grown_transcript", original=[msg("user", content="hi")], candidate=[msg("user", content="hi")],
    budget=None, io={"estimate_messages_tokens_rough": [100, 50, 50]})
add("salvage_grown_transcript", original=[msg("user")],
    candidate=[msg("user"), msg("tool", content="a" * 300), msg("tool", content="b" * 300), msg("tool", content="c" * 300)],
    budget=1000, io={"estimate_messages_tokens_rough": [2000, 500]})
add("salvage_grown_transcript", original=[msg("user")],
    candidate=[msg("user"), msg("assistant", _compressed_summary=True, content="x" * 8100 + "\n" + END_MARKER)],
    budget=100000, io={"elide_keeping_skill_safety_rule": "ELIDED", "estimate_messages_tokens_rough": [50000, 50000]})
add("salvage_grown_transcript", original=[msg("assistant")], candidate=[msg("assistant", content="x")],
    budget=1000, io={"estimate_messages_tokens_rough": [500]})
add("salvage_grown_transcript", original=[msg("user")], candidate=[msg("user")], budget=None,
    io={"estimate_messages_tokens_rough": raises("RuntimeError", "boom")})
add("salvage_grown_transcript", original=[msg("user")],
    candidate=[msg("user"), msg("assistant", _compressed_summary=True, content="x" * 8100 + "\n" + END_MARKER)],
    budget=100000, io={"elide_keeping_skill_safety_rule": raises("SystemExit", "bye")})
add("salvage_grown_transcript", original=[msg("user")], candidate=[msg("user")], budget=None, io={})
add("salvage_grown_transcript", original=[msg("user")],
    candidate=[msg("user"), msg("assistant", _compressed_summary=True, content="x" * 8100 + "\n" + END_MARKER)],
    budget=100000, io={"elide_keeping_skill_safety_rule": None, "estimate_messages_tokens_rough": [50000, 50000]})
add("salvage_grown_transcript", original=[msg("user")], candidate=[msg("user")], budget=None,
    io={"estimate_messages_tokens_rough": [100, 50, 200]})
add("salvage_grown_transcript", original=[msg("user")],
    candidate=[msg("user", content="task"), msg("user", _todo_snapshot_synthetic=True, content="todo")],
    budget=1000, io={"estimate_messages_tokens_rough": [2000, 500]})

# ── _archive_watermark_for ──
add("_archive_watermark_for", session_db=None, session_id="s1", held=[msg("user")], start_watermark=None,
    persistence=PERSIST, io={"persistence.messages.active_watermark": attr(last_active=5), "held_archive_watermark": 42})
add("_archive_watermark_for", session_db=SESSION_DB, session_id="s1", held=[], start_watermark=None,
    persistence=None, io={"session_db.get_active_message_watermark": 7, "held_archive_watermark": 7})
add("_archive_watermark_for", session_db=attr(get_active_message_watermark=callable_("session_db.get_active_message_watermark")),
    session_id="s1", held=[], start_watermark=None, persistence=None, io={})
add("_archive_watermark_for", session_db=None, session_id="s1", held=[], start_watermark=99,
    persistence=PERSIST, io={"held_archive_watermark": 99})
add("_archive_watermark_for", session_db=None, session_id="s1", held=[], start_watermark=None,
    persistence=PERSIST, io={"persistence.messages.active_watermark": attr(last_active=5),
                             "held_archive_watermark": raises("StaleHeldHistory", "stale")})
add("_archive_watermark_for", session_db=SESSION_DB, session_id="s1", held=[], start_watermark=3,
    persistence=None, io={"held_archive_watermark": raises("KeyboardInterrupt", "")})
add("_archive_watermark_for", session_db=None, session_id="s1", held=[], start_watermark=None,
    persistence=PERSIST, io={"persistence.messages.active_watermark": None})
add("_archive_watermark_for", session_db=None, session_id="s1", held=[], start_watermark=None,
    persistence=PERSIST, io={"persistence.messages.active_watermark": attr(last_active="12"), "held_archive_watermark": 12})
add("_archive_watermark_for", session_db=SESSION_DB, session_id="s1", held=[], start_watermark=5,
    persistence=None, io={})
add("_archive_watermark_for", session_db=None, session_id="s1", held=[], start_watermark=None,
    persistence=PERSIST, io={"persistence.messages.active_watermark": attr(last_active=12.9), "held_archive_watermark": 12})
add("_archive_watermark_for", session_db=None, session_id="s1", held=[], start_watermark=None,
    persistence=PERSIST, io={"persistence.messages.active_watermark": attr(last_active=None), "held_archive_watermark": 0})
add("_archive_watermark_for", session_db=None, session_id="s1", held=[], start_watermark=None,
    persistence=PERSIST, io={"persistence.messages.active_watermark": attr()})

# ── _exc_status_code ──
add("_exc_status_code", exc=attr(status_code=401))
add("_exc_status_code", exc=attr(status_code=0, response=attr(status_code=403)))
add("_exc_status_code", exc=attr(response=attr(status_code=402)))
add("_exc_status_code", exc=attr())
add("_exc_status_code", exc=None)
add("_exc_status_code", exc=attr(status_code="", response=attr(status_code=500)))
add("_exc_status_code", exc=attr(status_code=None, response=None))

# ── _is_summary_refusal ──
add("_is_summary_refusal", content="I cannot summarize this conversation.")
add("_is_summary_refusal", content="I can't summarize this.")
add("_is_summary_refusal", content="We refuse to summarize the checkpoint.")
add("_is_summary_refusal", content="I am unable to summarize.")
add("_is_summary_refusal", content="I'm unable to provide a summary.")
add("_is_summary_refusal", content="I cannot see earlier turns, but here is the summary.\n## Section")
add("_is_summary_refusal", content="This is a normal summary of the conversation.")
add("_is_summary_refusal", content="I cannot help with that.")
add("_is_summary_refusal", content="Sorry, I cannot summarize this.")
add("_is_summary_refusal", content="As an AI, I must decline to summarize.")
add("_is_summary_refusal", content="")
add("_is_summary_refusal", content=None)
add("_is_summary_refusal", content="I will not summarize.")
add("_is_summary_refusal", content="I could not summarize the checkpoint.")

# ── _response_refusal_text ──
add("_response_refusal_text", response=attr(), io={"_coerce_llm_message": attr(), "_message_field": "  refusal text  "})
add("_response_refusal_text", response=None, io={"_coerce_llm_message": None, "_message_field": {"message": "m"}})
add("_response_refusal_text", response=None, io={"_coerce_llm_message": None, "_message_field": {"reason": "r"}})
add("_response_refusal_text", response=None, io={"_coerce_llm_message": None, "_message_field": {"text": "t"}})
add("_response_refusal_text", response=None, io={"_coerce_llm_message": None, "_message_field": {}})
add("_response_refusal_text", response=None, io={"_coerce_llm_message": None, "_message_field": 5})
add("_response_refusal_text", response=None, io={"_coerce_llm_message": raises("ValueError", "x")})
add("_response_refusal_text", response=None, io={"_coerce_llm_message": None, "_message_field": raises("SystemExit", "bye")})
add("_response_refusal_text", response=None, io={})
add("_response_refusal_text", response=None, io={"_coerce_llm_message": None, "_message_field": "   "})

# ── _is_refusal_response ──
add("_is_refusal_response", response=None, content="I cannot summarize this.",
    io={"_coerce_llm_message": None, "_message_field": "explicit refusal"})
add("_is_refusal_response", response=None, content="I cannot summarize this.",
    io={"_coerce_llm_message": None, "_message_field": ""})
add("_is_refusal_response", response=None, content="a normal summary",
    io={"_coerce_llm_message": None, "_message_field": ""})
add("_is_refusal_response", response=None, content=None,
    io={"_coerce_llm_message": None, "_message_field": ""})
add("_is_refusal_response", response=None, content="I cannot summarize.",
    io={"_coerce_llm_message": raises("RuntimeError", "x")})

# ── _is_summary_stub ──
add("_is_summary_stub", content="x", io={"without_skill_safety_rule": "[terminal] ran ls (120 chars)"})
add("_is_summary_stub", content="x", io={"without_skill_safety_rule": "plain text", "is_demoted_stub": True})
add("_is_summary_stub", content="x", io={"without_skill_safety_rule": "plain text", "is_demoted_stub": False})
add("_is_summary_stub", content="x", io={"without_skill_safety_rule": "plain text", "is_demoted_stub": None})
add("_is_summary_stub", content="x", io={"without_skill_safety_rule": None})
add("_is_summary_stub", content="x", io={"without_skill_safety_rule": "[" + "a" * 400 + " chars)", "is_demoted_stub": False})
add("_is_summary_stub", content="x", io={})
add("_is_summary_stub", content="x", io={"without_skill_safety_rule": raises("GeneratorExit", "")})

# ── _skill_view_failed ──
add("_skill_view_failed", content='{"error":"boom"}')
add("_skill_view_failed", content='{"success":false}')
add("_skill_view_failed", content="[skill_view] name FAILED")
add("_skill_view_failed", content="[skill_view] name FAILED (120 chars)")
add("_skill_view_failed", content="[skill_view] name ok")
add("_skill_view_failed", content="plain")
add("_skill_view_failed", content=None)
add("_skill_view_failed", content='{"success":true}')

# ── _synthetic_user_row ──
add("_synthetic_user_row", content="[System: hello]")
add("_synthetic_user_row", content="[CONTEXT compaction]")
add("_synthetic_user_row", content="Cronjob Response: done")
add("_synthetic_user_row", content="real user words")
add("_synthetic_user_row", content="")
add("_synthetic_user_row", content="   ")
add("_synthetic_user_row", content="  [System: x]")
add("_synthetic_user_row", content=None)
add("_synthetic_user_row", content="[Your active task list]")

# ── _typed_user_text ──
add("_typed_user_text", content="/skill do X", io={"skills_loaded_by_scaffold": {"__script__": True, "ret": ["skill"]}, "describe_skill_invocation": "/skill do X"})
add("_typed_user_text", content="/skill do X", io={"skills_loaded_by_scaffold": {"__script__": True, "ret": ["skill"]}, "describe_skill_invocation": None})
add("_typed_user_text", content="plain text", io={"skills_loaded_by_scaffold": {"__script__": True, "ret": []}})
add("_typed_user_text", content="x", io={"skills_loaded_by_scaffold": raises("ValueError", "x")})
add("_typed_user_text", content="x", io={"skills_loaded_by_scaffold": {"__script__": True, "ret": ["s"]}, "describe_skill_invocation": raises("SystemExit", "bye")})
add("_typed_user_text", content="x", io={})
add("_typed_user_text", content=None, io={"skills_loaded_by_scaffold": {"__script__": True, "ret": []}})

# ── _json_dict ──
add("_json_dict", text='{"a":1}')
add("_json_dict", text="not json")
add("_json_dict", text="[1,2]")
add("_json_dict", text="")
add("_json_dict", text=None)
add("_json_dict", text="   ")
add("_json_dict", text="null")
add("_json_dict", text="123")
add("_json_dict", text='{"error":"x"}')
add("_json_dict", text=5)
add("_json_dict", text={"a": 1})

# ── _failure_suffix ──
add("_failure_suffix", reason="boom")
add("_failure_suffix", reason=None)
add("_failure_suffix", reason="  a   b  ")
add("_failure_suffix", reason="x" * 100)
add("_failure_suffix", reason="boom", label="SKIPPED")
add("_failure_suffix", reason=0)
add("_failure_suffix", reason=5)
add("_failure_suffix", reason=True)
add("_failure_suffix", reason="")

# ── _payload_failure_suffix ──
add("_payload_failure_suffix", payload={"error": "boom"})
add("_payload_failure_suffix", payload={"success": False})
add("_payload_failure_suffix", payload={"success": 0})
add("_payload_failure_suffix", payload={"success": True})
add("_payload_failure_suffix", payload={})
add("_payload_failure_suffix", payload={"error": "", "success": False})
add("_payload_failure_suffix", payload={"error": None, "success": False})

# ── _result_failure_suffix ──
add("_result_failure_suffix", content='{"error":"boom"}')
add("_result_failure_suffix", content='{"success":false}')
add("_result_failure_suffix", content='{"ok":true}')
add("_result_failure_suffix", content="invalid")
add("_result_failure_suffix", content="")


def main():
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    short = {f: n for f, n in counts.items() if n < 5}
    if short:
        raise SystemExit("funcs with <5 cases: %s" % short)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    print("wrote %d cases across %d funcs -> %s" % (len(CASES), len(counts), OUT))
    for fnc in sorted(counts):
        print("  %-32s %d" % (fnc, counts[fnc]))


if __name__ == "__main__":
    main()
