#!/usr/bin/env python3
"""Generate tests/fixtures/context_compressor_flow4.jsonl (>=5 cases/function).

Each case is {"func": <live_name>, "in": {...}}; in.io holds the universal-seam
scripts. Both the C harness and the LIVE oracle consume the same file.

FLOW4 ports 11 UNCLAIMED module functions (remainder beyond flow3):
  _effective_input_window, _model_threshold_key_rank, _is_text_only_content,
  _is_clarify_non_response, _restated_request_text, _authored_request_text,
  _pending_tool_round, _image_payload, _sum_skill_view,
  _collect_paths_from_jsonish, _sum_clarify.
"""
import json
from collections import Counter
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "context_compressor_flow4.jsonl"
cases = []


def case(func, **inp):
    cases.append({"func": func, "in": inp})


def exc(kind, msg="boom"):
    return {"__raise__": kind, "message": msg}


HDR = ("[STILL IN PROGRESS — this is the active request, restated after the "
       "compaction boundary because it was not finished yet. Continue it; do not "
       "start over.]")

# ── _effective_input_window (no seams) ────────────────────────────────────
case("_effective_input_window", context_length=100000, max_tokens=2000)
case("_effective_input_window", context_length=100000, max_tokens=None)
case("_effective_input_window", context_length=1000, max_tokens=2000)
case("_effective_input_window", context_length=100000, max_tokens=0)
case("_effective_input_window", context_length=500, max_tokens=-100)
case("_effective_input_window", context_length=100, max_tokens=100)
case("_effective_input_window", context_length=0, max_tokens=0)

# ── _model_threshold_key_rank (no seams) ──────────────────────────────────
case("_model_threshold_key_rank", key="astra", model="gpt-astra-x", provider="codex")
case("_model_threshold_key_rank", key="codex:astra", model="astra-1", provider="codex")
case("_model_threshold_key_rank", key="codex:astra", model="astra-1", provider="openrouter")
case("_model_threshold_key_rank", key="astra", model="no-match", provider="codex")
case("_model_threshold_key_rank", key="", model="anything", provider="codex")
case("_model_threshold_key_rank", key="codex:big", model="big-model", provider="CODEX")
case("_model_threshold_key_rank", key="codex:a:b", model="has a:b here", provider="codex")

# ── _is_text_only_content (no seams) ──────────────────────────────────────
case("_is_text_only_content", content="hello")
case("_is_text_only_content", content=[])
case("_is_text_only_content", content=["a", "b"])
case("_is_text_only_content", content=[{"type": "text", "text": "x"}])
case("_is_text_only_content", content=[{"type": "image_url"}])
case("_is_text_only_content", content=[{"type": "text", "text": 5}])
case("_is_text_only_content", content=42)
case("_is_text_only_content", content=[{"type": "input_text", "text": "y"}])

# ── _is_clarify_non_response (seam _clarify_headless_fullmatch) ───────────
case("_is_clarify_non_response", item="The user cancelled. Use your best judgement to proceed.")
case("_is_clarify_non_response", item="   The user did not provide a response because X")
case("_is_clarify_non_response", item=123)
case("_is_clarify_non_response", item="  a real answer  ",
     io={"_clarify_headless_fullmatch": False})
case("_is_clarify_non_response", item="headless answer",
     io={"_clarify_headless_fullmatch": True})
case("_is_clarify_non_response", item="  [oneshot mode: no user available. done]  ")
case("_is_clarify_non_response", item="", io={"_clarify_headless_fullmatch": False})
case("_is_clarify_non_response", item="normal",
     io={"_clarify_headless_fullmatch": exc("SystemExit", "bye")})

# ── _restated_request_text (seam _content_text_for_contains) ──────────────
case("_restated_request_text", content="any",
     io={"_content_text_for_contains": "  hello world  "})
case("_restated_request_text", content="any",
     io={"_content_text_for_contains": HDR + "\n do it now  "})
case("_restated_request_text", content="any",
     io={"_content_text_for_contains": HDR + " first " + HDR + "  final task  "})
case("_restated_request_text", content="any",
     io={"_content_text_for_contains": "   "})
case("_restated_request_text", content="any",
     io={"_content_text_for_contains": "x" + HDR})
case("_restated_request_text", content="any",
     io={"_content_text_for_contains": exc("KeyboardInterrupt", "stop")})

# ── _authored_request_text (seams _content_text_for_contains, _gateway_reply_pointer_sub) ──
case("_authored_request_text", content="x",
     io={"_content_text_for_contains": " task text ",
         "_gateway_reply_pointer_sub": " task text "})
case("_authored_request_text", content="x",
     io={"_content_text_for_contains": HDR + "  do thing  ",
         "_gateway_reply_pointer_sub": "real task  "})
case("_authored_request_text", content="x",
     io={"_content_text_for_contains": "[Replying to: \"old\"]\n\nnew ask  ",
         "_gateway_reply_pointer_sub": "new ask  "})
case("_authored_request_text", content="x",
     io={"_content_text_for_contains": "   ",
         "_gateway_reply_pointer_sub": ""})
case("_authored_request_text", content="x",
     io={"_content_text_for_contains": "plain",
         "_gateway_reply_pointer_sub": exc("ValueError", "bad")})

# ── _pending_tool_round (no seams; STEER_DISPLAY_KIND = "steer") ──────────
case("_pending_tool_round", messages=[])
case("_pending_tool_round", messages=[{"role": "user"}, {"role": "assistant"}])
case("_pending_tool_round", messages=[{"role": "user"}, {"role": "assistant"},
                                      {"role": "tool"}, {"role": "tool"}])
case("_pending_tool_round", messages=[{"role": "user"}, {"role": "assistant"},
                                      {"role": "tool"}, {"display_kind": "steer"}])
case("_pending_tool_round", messages=[{"role": "user"}, {"role": "assistant"},
                                      {"role": "tool"}, {"display_kind": "steer"},
                                      {"display_kind": "steer"}])
case("_pending_tool_round", messages=[{"role": "user"}, {"role": "tool", "display_kind": "steer"},
                                      {"role": "tool"}])
case("_pending_tool_round", messages=[{"role": "tool"}, {"role": "tool"}, {"role": "user"}])

# ── _image_payload (seams _tool_result_parts, _is_image_part) ─────────────
case("_image_payload", msg={"content": "plain"})
case("_image_payload", msg={"content": {"_multimodal": True, "content": []}},
     io={"_tool_result_parts": []})
case("_image_payload", msg={"content": "m"},
     io={"_tool_result_parts": [
         {"type": "image_url", "image_url": {"url": "data:image/png;base64,AAAA"}},
         {"type": "text", "text": "hi"}],
         "_is_image_part": [True, False]})
case("_image_payload", msg={"content": "m"},
     io={"_tool_result_parts": [
         {"type": "input_image", "source": {"data": "BASE64DATA"}},
         {"type": "image", "image_url": "https://x.io/img.png"}],
         "_is_image_part": [True, True]})
case("_image_payload", msg={"content": "m"},
     io={"_tool_result_parts": [{"type": "image_url"}, {"type": "image"}],
         "_is_image_part": [True, True]})
case("_image_payload", msg={"content": "m"},
     io={"_tool_result_parts": [{"type": "text"}],
         "_is_image_part": exc("FileNotFoundError", "missing")})

# ── _sum_skill_view ───────────────────────────────────────────────────────
case("_sum_skill_view", name="skill-a", args={"name": "skill-a"}, content="ok",
     content_len=100, line_count=1,
     io={"_result_failure_suffix": "", "_skill_view_failed": False,
         "_is_summary_stub": False,
         "_skill_pruned_marker": "[SKILL_PRUNED: skill-a] lost"})
case("_sum_skill_view", name="anon", args={}, content="ok", content_len=5, line_count=1,
     io={"_result_failure_suffix": "", "_skill_view_failed": False,
         "_is_summary_stub": False})
case("_sum_skill_view", name="fail", args={"name": "fail"}, content="bad", content_len=10,
     line_count=1,
     io={"_result_failure_suffix": " FAILED: boom", "_is_summary_stub": False})
case("_sum_skill_view", name="stub", args={"name": "stub"}, content="stub", content_len=7,
     line_count=1,
     io={"_result_failure_suffix": "", "_skill_view_failed": True,
         "_is_summary_stub": False})
case("_sum_skill_view", name="stub2", args={"name": "stub2"},
     content="[SKILL_PRUNED: stub2]  (9 chars)", content_len=9, line_count=1,
     io={"_result_failure_suffix": "", "_skill_view_failed": False,
         "_is_summary_stub": True,
         "_skill_pruned_marker": "[SKILL_PRUNED: stub2] lost"})
case("_sum_skill_view", name="num", args={"name": 7}, content="ok", content_len=9,
     line_count=1,
     io={"_result_failure_suffix": "", "_skill_view_failed": False,
         "_is_summary_stub": False,
         "_skill_pruned_marker": "[SKILL_PRUNED: 7] lost"})
case("_sum_skill_view", name="exc", args={"name": "exc"}, content="x", content_len=1,
     line_count=1,
     io={"_result_failure_suffix": exc("ModuleNotFoundError", "no module")})

# ── _collect_paths_from_jsonish (seam _path_mention_findall; mutates relevant_files) ──
case("_collect_paths_from_jsonish", obj={"path": "/a/b.txt",
     "nested": {"workdir": " /repo ", "x": [1, "see /tmp/x.py"]}},
     relevant_files=[], io={"_path_mention_findall": {"__script__": True, "ret": []}})
case("_collect_paths_from_jsonish", obj={"other": "/no", "file_path": "/yes.py"},
     relevant_files=[], io={"_path_mention_findall": {"__script__": True, "ret": []}})
case("_collect_paths_from_jsonish", obj=[" /one.py ", {"output_path": "/two.py"}],
     relevant_files=["/seed.py"], io={"_path_mention_findall": {"__script__": True, "ret": []}})
case("_collect_paths_from_jsonish", obj="look at /docs/readme.md and ~/.ssh/id_rsa",
     relevant_files=[], io={"_path_mention_findall":
         {"__script__": True, "ret": ["/docs/readme.md.", "~/.ssh/id_rsa"]}})
case("_collect_paths_from_jsonish", obj={"path": "/dup.py"}, relevant_files=["/dup.py"],
     io={"_path_mention_findall": {"__script__": True, "ret": []}})
case("_collect_paths_from_jsonish", obj={"path": "/p"}, relevant_files=[],
     io={"_path_mention_findall": exc("PermissionError", "denied")})

# ── _sum_clarify (seams _json_dict, _filter_legacy_clarify_answers, _json_dumps, _elide) ──
case("_sum_clarify", name="clarify", args={},
     content='{"responses":[{"status":"answered","user_response":"yes"}]}',
     content_len=40, line_count=1,
     io={"_json_dict": {"responses": [{"status": "answered", "user_response": "yes"}]},
         "_json_dumps": '"yes"', "_elide": "[clarify] user responded: \"yes\""})
case("_sum_clarify", name="clarify", args={},
     content='{"responses":[{"status":"answered","user_response":"a"},'
             '{"status":"answered","user_response":"b"}]}',
     content_len=80, line_count=1,
     io={"_json_dict": {"responses": [{"status": "answered", "user_response": "a"},
                                       {"status": "answered", "user_response": "b"}]},
         "_json_dumps": '["a", "b"]',
         "_elide": "[clarify] user responded: [\"a\", \"b\"]"})
case("_sum_clarify", name="clarify", args={},
     content='{"user_response":"legacy"}', content_len=20, line_count=1,
     io={"_json_dict": {"user_response": "legacy"},
         "_filter_legacy_clarify_answers": ["legacy"],
         "_json_dumps": '"legacy"', "_elide": "[clarify] user responded: \"legacy\""})
case("_sum_clarify", name="clarify", args={}, content='{"responses":[]}',
     content_len=2, line_count=1, io={"_json_dict": {"responses": []}})
case("_sum_clarify", name="clarify", args={}, content='{}',
     content_len=0, line_count=0, io={"_json_dict": {}})
case("_sum_clarify", name="clarify", args={},
     content='{"responses":[{"status":"answered","user_response":123}]}',
     content_len=30, line_count=1,
     io={"_json_dict": {"responses": [{"status": "answered", "user_response": 123}]},
         "_filter_legacy_clarify_answers": [123],
         "_elide": "[clarify] asked user a question"})
case("_sum_clarify", name="clarify", args={}, content='{"responses":[]}',
     content_len=2, line_count=1,
     io={"_json_dict": exc("CancelledError", "cancelled")})

# ── full builtin exception hierarchy — base-only + Exception families ─────
case("_effective_input_window", context_length=10, max_tokens=1,
     io={"_unused": exc("GeneratorExit", "gen")})
case("_is_clarify_non_response", item="x",
     io={"_clarify_headless_fullmatch": exc("CancelledError", "cancelled")})
case("_restated_request_text", content="x",
     io={"_content_text_for_contains": exc("CancelledError", "cancelled")})
case("_image_payload", msg={"content": "m"},
     io={"_tool_result_parts": {"__script__": True, "ret": [{"type": "image_url"}]},
         "_is_image_part": exc("ZeroDivisionError", "div0")})
case("_sum_skill_view", name="e", args={"name": "e"}, content="x", content_len=1,
     line_count=1, io={"_result_failure_suffix": "",
                       "_skill_view_failed": exc("PermissionError", "denied")})
case("_image_payload", msg={"content": "m"},
     io={"_tool_result_parts": exc("TimeoutError", "timeout")})
case("_sum_clarify", name="c", args={}, content="{}", content_len=0, line_count=0,
     io={"_json_dict": {}, "_filter_legacy_clarify_answers": exc("RuntimeError", "rt")})
case("_sum_clarify", name="c", args={}, content="{}", content_len=0, line_count=0,
     io={"_json_dict": {}, "_filter_legacy_clarify_answers": ["ok"],
         "_json_dumps": '"ok"', "_elide": exc("ValueError", "elide bad")})

OUT.parent.mkdir(parents=True, exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in cases:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")

cnt = Counter(c["func"] for c in cases)
assert len(cnt) == 11, cnt
assert all(cnt[f] >= 5 for f in cnt), cnt
print("wrote %d cases across %d functions -> %s" % (len(cases), len(cnt), OUT))
print("per-function counts:", dict(sorted(cnt.items())))
