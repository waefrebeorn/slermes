#!/usr/bin/env python3
"""Generate tests/fixtures/display_flow.jsonl.

Inputs only (no expected values): the oracle recomputes the live reference.
Scripted seam results live in in.io (see the harness header). Representation
scope: ASCII text inputs; label doubles carry {name,emoji,text,preview};
_CuteRow results are {"__cute_row__":true,...}; _detect_tool_failure results are
[is_failure, suffix]; no integral-float JSON literals / non-finite floats.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "display_flow.jsonl")
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def script(ret=None, raise_spec=None):
    s = {"__script__": True, "ret": ret}
    if raise_spec is not None:
        s["raise"] = raise_spec
    return s


def cute(emoji="E", verb="V", detail="D", text="ROW"):
    return {"__cute_row__": True, "text": text, "emoji": emoji, "verb": verb, "detail": detail}


def label(name="n", emoji="e", text="t", preview="p"):
    return {"name": name, "emoji": emoji, "text": text, "preview": preview}


RAISE_EXC = {"type": "RuntimeError", "message": "boom"}
RAISE_KBI = {"type": "KeyboardInterrupt", "message": "interrupted"}

# ── _memory_first_op ──────────────────────────────────────────────────────
add("_memory_first_op", args={})
add("_memory_first_op", args={"operations": []})
add("_memory_first_op", args={"operations": "x"})
add("_memory_first_op", args={"operations": [{"action": "add"}]})
add("_memory_first_op", args={"operations": [{"action": "add"}, {"action": "remove"}]}, io={"t": "N"})
add("_memory_first_op", args={"operations": [5]}, io={})
add("_memory_first_op", args={"operations": [{"action": "add"}, {"action": "remove"}]},
    io={"t": script(raise_spec=RAISE_EXC)})

# ── tool_labels_for_call ──────────────────────────────────────────────────
add("tool_labels_for_call", tool_name="x", args={"a": 1},
    io={"labels_for_call": script(raise_spec=RAISE_EXC), "logger.debug": script(None)})
add("tool_labels_for_call", tool_name="x", args=None,
    io={"labels_for_call": script(raise_spec=RAISE_KBI)})
add("tool_labels_for_call", tool_name="x", args={"a": 1},
    io={"labels_for_call": [[label()]], "_get_skin": None})
add("tool_labels_for_call", tool_name="x", args={"a": 1},
    io={"labels_for_call": [[label(name="n")]], "_get_skin": {"tool_emojis": {"n": "X"}}})
add("tool_labels_for_call", tool_name="x", args={"a": 1},
    io={"labels_for_call": [[label(name="n")]], "_get_skin": {"tool_emojis": {}}})
add("tool_labels_for_call", tool_name="x", args=None,
    io={"labels_for_call": [[label(), label(name="m", emoji="f")]], "_get_skin": {"tool_emojis": {"m": "Z"}}})
add("tool_labels_for_call", tool_name="x", args={}, io={})  # fail-closed both seams

# ── bridge_tool_labels ────────────────────────────────────────────────────
add("bridge_tool_labels", tool_name="x", args={"a": 1}, state={"_friendly_tool_labels": False}, io={})
add("bridge_tool_labels", tool_name="x", args={"a": 1}, io={"tool_labels_for_call": [[label()]]})
add("bridge_tool_labels", tool_name="x", args={"a": 1}, io={"tool_labels_for_call": [[]]})
add("bridge_tool_labels", tool_name="x", args=None, io={"tool_labels_for_call": [[label()]]})
add("bridge_tool_labels", tool_name="x", args={}, io={"tool_labels_for_call": script(raise_spec=RAISE_EXC)})
add("bridge_tool_labels", tool_name="x", args={}, state={"_friendly_tool_labels": True},
    io={"tool_labels_for_call": [[label(name="a"), label(name="b")]]})

# ── tool_row_emoji ────────────────────────────────────────────────────────
add("tool_row_emoji", tool_name="x", args=None, io={"get_tool_emoji": "EM"})
add("tool_row_emoji", tool_name="x", args={"a": 1}, io={"bridge_tool_labels": [[]], "get_tool_emoji": "EM"})
add("tool_row_emoji", tool_name="x", args={"a": 1}, io={"bridge_tool_labels": [[label(emoji="LE")]]})
add("tool_row_emoji", tool_name="x", args=None, default="DF", io={"get_tool_emoji": "EM"})
add("tool_row_emoji", tool_name="x", args={"a": 1}, io={"bridge_tool_labels": script(raise_spec=RAISE_EXC)})
add("tool_row_emoji", tool_name="x", args={}, io={"get_tool_emoji": "EM"})

# ── _cute_row ─────────────────────────────────────────────────────────────
add("_cute_row", emoji="\U0001f4c4", verb="fetch", detail="d", io={"t": "LBL"})
add("_cute_row", emoji="\U0001f4c4", verb="fetch", detail="d", io={"t": "abcdefghij"})
add("_cute_row", emoji=" \U0001f4c4 ", verb="fetch", detail="", io={"t": "LBL"})
add("_cute_row", emoji="E", verb="v", detail="detail text", io={"t": "x"})
add("_cute_row", emoji="E", verb="v", detail="d", io={"t": script(raise_spec=RAISE_EXC)})
add("_cute_row", emoji="E", verb="fetch", detail="d", io={"t": ""})

# ── _cute_fallback_row ────────────────────────────────────────────────────
add("_cute_fallback_row", tool_name="web_search", args={"q": 1},
    io={"build_tool_preview": "PV", "_cute_trunc": "TR", "get_tool_emoji": "EM"})
add("_cute_fallback_row", tool_name="web_search", args={},
    io={"build_tool_preview": None, "_cute_trunc": "TR", "get_tool_emoji": "EM"})
add("_cute_fallback_row", tool_name="ab", args={},
    io={"build_tool_preview": "PV", "_cute_trunc": "TR", "get_tool_emoji": "EM"})
add("_cute_fallback_row", tool_name="x", args={},
    io={"build_tool_preview": script(raise_spec=RAISE_EXC)})
add("_cute_fallback_row", tool_name="x", args={},
    io={"build_tool_preview": "PV", "_cute_trunc": "TR", "get_tool_emoji": script(raise_spec=RAISE_EXC)})
add("_cute_fallback_row", tool_name="abcdefghijk", args={},
    io={"build_tool_preview": "PV", "_cute_trunc": "TR", "get_tool_emoji": "EM"})

# ── _cute_web_extract ─────────────────────────────────────────────────────
add("_cute_web_extract", args={}, io={"t": "PAGES", "_cute_row": cute()})
add("_cute_web_extract", args={"urls": ["http://a.com"]},
    io={"_display_url": "http://a.com", "_domain": "a.com", "_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_web_extract", args={"urls": ["u1", "u2"]},
    io={"_display_url": "u1", "_domain": "d", "_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_web_extract", args={"urls": "scalar"},
    io={"_display_url": "scalar", "_domain": "d", "_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_web_extract", args={"urls": [""]},
    io={"_display_url": "", "t": "PAGES", "_cute_row": cute()})
add("_cute_web_extract", args={"urls": ["u"]},
    io={"_display_url": script(raise_spec=RAISE_EXC)})

# ── _cute_todo_list ───────────────────────────────────────────────────────
add("_cute_todo_list", args={"todos": None}, result=None, io={"t": "X", "_cute_row": cute()})
add("_cute_todo_list", args={"todos": [1, 2]}, result={"summary": {"total": 3, "completed": 1}},
    io={"safe_json_loads": {"summary": {"total": 3, "completed": 1}}, "t": "PROG", "_cute_row": cute()})
add("_cute_todo_list", args={"todos": [1], "merge": True}, result={"s": 1},
    io={"safe_json_loads": {"summary": {"total": 2, "completed": 1}}, "t": "UP", "_cute_row": cute()})
add("_cute_todo_list", args={"todos": [1, 2, 3], "merge": True}, result=None,
    io={"t": "UC", "_cute_row": cute()})
add("_cute_todo_list", args={"todos": None}, result={"s": 1},
    io={"safe_json_loads": {"summary": {"total": 5, "completed": 0}}, "t": "PROG", "_cute_row": cute()})
add("_cute_todo_list", args={"todos": [1]}, result="str",
    io={"safe_json_loads": script(raise_spec={"type": "ValueError", "message": "bad"}), "t": "X", "_cute_row": cute()})
add("_cute_todo_list", args={"todos": [1]}, result="str",
    io={"safe_json_loads": script(raise_spec=RAISE_KBI)})
add("_cute_todo_list", args={"todos": [1]}, result=None, io={"t": "CNT", "_cute_row": cute()})

# ── _cute_memory ──────────────────────────────────────────────────────────
add("_cute_memory", args={"operations": [{"action": "add", "content": "c"}], "target": "T"},
    io={"_memory_first_op": {"op": {"action": "add", "content": "c"}, "more": ""}, "_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_memory", args={"target": "T"},
    io={"_memory_first_op": {"op": {"action": "replace", "old_text": "o"}, "more": ""}, "_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_memory", args={"target": "T"},
    io={"_memory_first_op": {"op": {"action": "remove", "old_text": "o"}, "more": " (2)"}, "_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_memory", args={"target": "T"},
    io={"_memory_first_op": {"op": {"action": "patch"}, "more": ""}, "t": "MISS", "_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_memory", args={},
    io={"_memory_first_op": {"op": {"action": "other"}, "more": ""}, "_cute_row": cute()})
add("_cute_memory", args={"target": "T"},
    io={"_memory_first_op": {"op": {"action": "add", "new_text": "nt"}, "more": ""}, "_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_memory", args={}, io={"_memory_first_op": script(raise_spec=RAISE_EXC)})

# ── _cute_skill_view ──────────────────────────────────────────────────────
add("_cute_skill_view", args={"name": "n", "file_path": "f"}, io={"_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_skill_view", args={"name": "n"}, io={"_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_skill_view", args={"file_path": "f"}, io={"_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_skill_view", args={}, io={"_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_skill_view", args={"name": "n"}, io={"_cute_trunc": script(raise_spec=RAISE_EXC)})
add("_cute_skill_view", args={"name": "", "file_path": "f"}, io={"_cute_trunc": "TR", "_cute_row": cute()})

# ── _cute_cronjob ─────────────────────────────────────────────────────────
add("_cute_cronjob", args={"action": "create", "name": "nm"}, io={"t": "X", "_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_cronjob", args={"action": "create", "skills": ["s1", "s2"]}, io={"t": "X", "_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_cronjob", args={"action": "create", "skill": "sk"}, io={"t": "X", "_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_cronjob", args={"action": "create", "prompt": "pr"}, io={"t": "X", "_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_cronjob", args={"action": "create"}, io={"t": "FB", "_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_cronjob", args={"action": "list"}, io={"t": "LIST", "_cute_row": cute()})
add("_cute_cronjob", args={"action": "pause", "job_id": "j1"}, io={"_cute_row": cute()})
add("_cute_cronjob", args={"action": "create", "name": "nm"}, io={"t": "X", "_cute_trunc": script(raise_spec=RAISE_EXC)})

# ── _cute_execute_code ────────────────────────────────────────────────────
add("_cute_execute_code", args={"code": "line1\nline2"}, io={"_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_execute_code", args={"code": ""}, io={"_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_execute_code", args={"code": "   "}, io={"_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_execute_code", args={}, io={"_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_execute_code", args={"code": None}, io={})  # AttributeError on .strip
add("_cute_execute_code", args={"code": "x"}, io={"_cute_trunc": script(raise_spec=RAISE_EXC)})

# ── _cute_browser_exec ────────────────────────────────────────────────────
add("_cute_browser_exec", args={"code": "# Step\ncode"}, io={"_browser_exec_step_label": "Step", "_cute_row": cute()})
add("_cute_browser_exec", args={"code": "code"},
    io={"_browser_exec_step_label": None, "_oneline": "OL", "_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_browser_exec", args={},
    io={"_browser_exec_step_label": None, "_oneline": "OL", "_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_browser_exec", args={"code": "x"}, io={"_browser_exec_step_label": script(raise_spec=RAISE_EXC)})
add("_cute_browser_exec", args={"code": "x"},
    io={"_browser_exec_step_label": None, "_oneline": script(raise_spec=RAISE_EXC)})
add("_cute_browser_exec", args={"code": None},
    io={"_browser_exec_step_label": None, "_oneline": "OL", "_cute_trunc": "TR", "_cute_row": cute()})

# ── _cute_delegate ────────────────────────────────────────────────────────
add("_cute_delegate", args={"action": "list"},
    io={"_delegate_action_preview": "list abc", "_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_delegate", args={"tasks": [{"goal": "g1"}, {"goal": "g2"}]},
    io={"_delegate_action_preview": None, "_delegate_task_goals": [["g1", "g2"]], "_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_delegate", args={"tasks": [{}, {}]},
    io={"_delegate_action_preview": None, "_delegate_task_goals": [[]], "t": "PAR", "_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_delegate", args={"goal": "do it"},
    io={"_delegate_action_preview": None, "_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_delegate", args={"goal": None},
    io={"_delegate_action_preview": None, "_cute_trunc": "TR", "_cute_row": cute()})
add("_cute_delegate", args={"tasks": [{"goal": "g"}]},
    io={"_delegate_action_preview": None, "_delegate_task_goals": script(raise_spec=RAISE_EXC)})

# ── _cute_process_manage ──────────────────────────────────────────────────
add("_cute_process_manage", args={"action": "list"}, io={"t": "LIST", "_cute_row": cute()})
add("_cute_process_manage", args={"action": "wait", "session_id": "abcdefghijklmnop"}, io={"_cute_row": cute()})
add("_cute_process_manage", args={"action": "poll"}, io={"_cute_row": cute()})
add("_cute_process_manage", args={"action": "wait", "session_id": None}, io={})  # TypeError subscript
add("_cute_process_manage", args={"action": "kill", "session_id": "short"}, io={"_cute_row": cute()})
add("_cute_process_manage", args={"action": "list"}, io={"t": script(raise_spec=RAISE_EXC)})

# ── _cute_scroll ──────────────────────────────────────────────────────────
add("_cute_scroll", args={"direction": "down"}, io={"t": "DOWN", "_cute_row": cute()})
add("_cute_scroll", args={"direction": "up"}, io={"t": "UP", "_cute_row": cute()})
add("_cute_scroll", args={"direction": "left"}, io={"t": "LEFT", "_cute_row": cute()})
add("_cute_scroll", args={"direction": "diag"}, io={"_cute_row": cute()})
add("_cute_scroll", args={}, io={"t": "DOWN", "_cute_row": cute()})
add("_cute_scroll", args={"direction": "right"}, io={"t": script(raise_spec=RAISE_EXC)})

# ── _cute_bridge_rows ─────────────────────────────────────────────────────
add("_cute_bridge_rows", tool_name="x", args={}, io={"bridge_tool_labels": [[]]})
add("_cute_bridge_rows", tool_name="x", args={}, io={"bridge_tool_labels": [[label()]], "_cute_trunc": "TR"})
add("_cute_bridge_rows", tool_name="x", args={}, io={"bridge_tool_labels": [[label(preview="")]], "_cute_trunc": "TR"})
add("_cute_bridge_rows", tool_name="x", args={},
    io={"bridge_tool_labels": [[label(name="a"), label(name="b", preview="")]], "_cute_trunc": "TR"})
add("_cute_bridge_rows", tool_name="x", args={}, io={"bridge_tool_labels": script(raise_spec=RAISE_EXC)})
add("_cute_bridge_rows", tool_name="x", args=None, io={"bridge_tool_labels": [[label(emoji="EM", text="TX")]] , "_cute_trunc": "TR"})

# ── _entry_failure_suffix ─────────────────────────────────────────────────
add("_entry_failure_suffix", entry="notdict", io={})
add("_entry_failure_suffix", entry={"error": {"message": "m"}}, io={"_trim_error": "trimmed"})
add("_entry_failure_suffix", entry={"error": {"code": "c"}}, io={"_trim_error": "trimmed"})
add("_entry_failure_suffix", entry={"error": {}}, io={"t": "GW", "_trim_error": "trimmed"})
add("_entry_failure_suffix", entry={"error": "boom", "name": "t"}, io={"_detect_tool_failure": [[True, " [d]"]]})
add("_entry_failure_suffix", entry={"success": False}, io={"_detect_tool_failure": [[True, " [d]"]]})
add("_entry_failure_suffix", entry={}, io={})
add("_entry_failure_suffix", entry={"error": {"message": "m"}}, io={"_trim_error": script(raise_spec=RAISE_EXC)})

# ── _bridge_row_suffixes ──────────────────────────────────────────────────
add("_bridge_row_suffixes", rows=2, result={"results": [{"error": "boom", "name": "t"}, {"success": False, "name": "u"}]},
    call_suffix="", io={"_entry_failure_suffix": [" [e1]", " [e2]"]})
add("_bridge_row_suffixes", rows=1, result=None, call_suffix="calls[0]", io={"safe_json_loads": None})
add("_bridge_row_suffixes", rows=2, result="jsonstr", call_suffix="",
    io={"safe_json_loads": {"results": [{"error": "x", "index": 1}]}, "_entry_failure_suffix": [" [e]"]})
add("_bridge_row_suffixes", rows=2, result={"results": [{"error": "x", "index": 5}]}, call_suffix="",
    io={"_entry_failure_suffix": [" [e]"]})
add("_bridge_row_suffixes", rows=2, result=None, call_suffix="xx", io={"safe_json_loads": None})
add("_bridge_row_suffixes", rows=2, result={"results": [{"error": "x"}]}, call_suffix="calls[9]",
    io={"_entry_failure_suffix": [" [e]"]})
add("_bridge_row_suffixes", rows=1, result={"results": [{"error": "x"}]}, call_suffix="",
    io={"_entry_failure_suffix": script(raise_spec=RAISE_KBI)})
add("_bridge_row_suffixes", rows=0, result=None, call_suffix="", io={"safe_json_loads": None})

# ── get_tool_row_parts ────────────────────────────────────────────────────
add("get_tool_row_parts", tool_name="x", args={"q": 1}, result=None, is_error=False,
    io={"redact_tool_args_for_display": None, "_detect_tool_failure": [[False, ""]],
        "bridge_tool_labels": [[label(), label(name="b")]], "_bridge_row_suffixes": [["", ""]], "_cute_trunc": "TR"})
add("get_tool_row_parts", tool_name="web_search", args={"query": "x"}, result=None,
    io={"redact_tool_args_for_display": None, "_detect_tool_failure": [[False, ""]],
        "bridge_tool_labels": [[]], "cute_render": cute(emoji="\U0001f50d", verb="search", detail="d")})
add("get_tool_row_parts", tool_name="web_search", args={"query": "x"}, result=None,
    io={"redact_tool_args_for_display": None, "_detect_tool_failure": [[False, ""]],
        "bridge_tool_labels": [[]], "cute_render": "plainstr", "_cute_fallback_row": cute()})
add("get_tool_row_parts", tool_name="custom_tool", args={"a": 1}, result=None,
    io={"redact_tool_args_for_display": None, "_detect_tool_failure": [[False, ""]],
        "bridge_tool_labels": [[]], "_cute_fallback_row": cute()})
add("get_tool_row_parts", tool_name="x", args={"a": 1}, result=None,
    io={"redact_tool_args_for_display": None, "_detect_tool_failure": script(raise_spec=RAISE_EXC), "logger.debug": script(None)})
add("get_tool_row_parts", tool_name="x", args={"a": 1}, result=None,
    io={"redact_tool_args_for_display": None, "_detect_tool_failure": script(raise_spec=RAISE_KBI)})
add("get_tool_row_parts", tool_name="x", args={"q": 1}, result=None, is_error=True,
    io={"redact_tool_args_for_display": None, "_detect_tool_failure": [[False, ""]],
        "bridge_tool_labels": [[label()]], "_bridge_row_suffixes": [[""]], "_cute_trunc": "TR"})
add("get_tool_row_parts", tool_name="x", args={"q": 1}, result=None,
    io={"redact_tool_args_for_display": None, "_detect_tool_failure": [[True, " [exit 1]"]],
        "bridge_tool_labels": [[label(), label(name="b")]], "_bridge_row_suffixes": [["", ""]], "_cute_trunc": "TR"})
add("get_tool_row_parts", tool_name="x", args=None, result=None,
    io={"redact_tool_args_for_display": {"z": 2}, "_detect_tool_failure": [[False, ""]],
        "bridge_tool_labels": [[]], "_cute_fallback_row": cute()})

# ── _skin_spinner_list ────────────────────────────────────────────────────
add("_skin_spinner_list", key="waiting_faces", fallback=["a"], io={"_get_skin": None})
add("_skin_spinner_list", key="waiting_faces", fallback=["a"], io={"_get_skin": {"spinner": {"waiting_faces": ["x"]}}})
add("_skin_spinner_list", key="waiting_faces", fallback=["a"], io={"_get_skin": {"spinner": {"waiting_faces": []}}})
add("_skin_spinner_list", key="waiting_faces", fallback=["a"], io={"_get_skin": script(raise_spec=RAISE_EXC)})
add("_skin_spinner_list", key="waiting_faces", fallback=["a"], io={"_get_skin": script(raise_spec=RAISE_KBI)})
add("_skin_spinner_list", key="waiting_faces", fallback=["a"], io={"_get_skin": {"tool_emojis": {}}})

# ── _catalog_thinking_verbs ───────────────────────────────────────────────
add("_catalog_thinking_verbs", io={"t": "a, b ,c"})
add("_catalog_thinking_verbs", io={"t": ""})
add("_catalog_thinking_verbs", io={"t": "display.thinking_verbs"})
add("_catalog_thinking_verbs", io={"t": " , , "})
add("_catalog_thinking_verbs", io={"t": "pondering"})
add("_catalog_thinking_verbs", io={"t": script(raise_spec=RAISE_EXC)})

# ── _clear_line_blanks ────────────────────────────────────────────────────
add("_clear_line_blanks", state={"last_line_len": 0})
add("_clear_line_blanks", state={"last_line_len": 10})
add("_clear_line_blanks", state={"last_line_len": 35})
add("_clear_line_blanks", state={"last_line_len": 40})
add("_clear_line_blanks", state={"last_line_len": 100})
add("_clear_line_blanks", state={})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
    # per-function count check (>=5 each)
    from collections import Counter
    cnt = Counter(c["func"] for c in CASES)
    short = {k: v for k, v in cnt.items() if v < 5}
    print(f"Wrote {len(CASES)} cases across {len(cnt)} funcs to {OUT}")
    if short:
        print("WARNING funcs with <5 cases:", short)
    else:
        print("All funcs have >=5 cases.")


if __name__ == "__main__":
    main()