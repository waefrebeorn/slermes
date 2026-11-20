#!/usr/bin/env python3
"""Generate tests/fixtures/context_compressor_flow3.jsonl (>=5 cases/function).

Each case is {"func": <live_name>, "in": {...}}; in.io holds the universal-seam
scripts. Both the C harness and the LIVE oracle consume the same file.
"""
import json
from pathlib import Path

OUT = Path(__file__).resolve().parent / "fixtures" / "context_compressor_flow3.jsonl"
cases = []


def case(func, **inp):
    cases.append({"func": func, "in": inp})


def common(args=None, content="", content_len=0, line_count=0, name="tool", io=None):
    d = {"name": name, "args": args or {}, "content": content,
         "content_len": content_len, "line_count": line_count}
    if io is not None:
        d["io"] = io
    return d


# ── _sum_terminal (seam _str_arg x1) ──────────────────────────────────────
case("_sum_terminal", **common({"command": "ls -la"}, '{"exit_code":0}', 12, 3,
      io={"_str_arg": "ls -la"}))
case("_sum_terminal", **common({"command": "x" * 90}, '{"exit_code": 2}', 90, 1,
      io={"_str_arg": "x" * 90}))
case("_sum_terminal", **common({"command": "echo hi"}, "no status here", 6, 1,
      io={"_str_arg": "echo hi"}))
case("_sum_terminal", **common({"command": "false"}, '{"exit_code"  :  -1}', 4, 0,
      io={"_str_arg": "false"}))
case("_sum_terminal", **common({"command": "echo spaced"}, '{"exit_code" : 42}', 11, 2,
      io={"_str_arg": "echo spaced"}))
case("_sum_terminal", **common({"command": ""}, '{"exit_code":0}', 0, 0,
      io={"_str_arg": ""}))

# ── _sum_write_file (seams _result_failure_suffix, _str_arg(conditional)) ──
case("_sum_write_file", **common({"path": "/a/b.py", "content": "a\nb\nc"}, "ok", 6, 3,
      io={"_result_failure_suffix": "", "_str_arg": "a\nb\nc"}))
case("_sum_write_file", **common({"path": "/x.py", "content": "hi"}, "err", 2, 1,
      io={"_result_failure_suffix": " FAILED: disk full"}))
case("_sum_write_file", **common({"path": "/no.py"}, "ok", 0, 0,
      io={"_result_failure_suffix": ""}))
case("_sum_write_file", **common({"path": "/e.py", "content": ""}, "ok", 0, 0,
      io={"_result_failure_suffix": ""}))
case("_sum_write_file", **common({"path": "/one.py", "content": "only"}, "ok", 4, 1,
      io={"_result_failure_suffix": "", "_str_arg": "only"}))
case("_sum_write_file", **common({}, "ok", 0, 0,
      io={"_result_failure_suffix": ""}))

# ── _sum_search_files (no seams) ──────────────────────────────────────────
case("_sum_search_files", **common({"target": "files", "pattern": "TODO", "path": "/repo"},
      '{"total_count":12}', 120, 12))
case("_sum_search_files", **common({"pattern": "x"}, '{"total_count" : 0}', 0, 0))
case("_sum_search_files", **common({"pattern": "y", "path": "/p"}, "nothing", 5, 0))
case("_sum_search_files", **common({"target": "content", "pattern": "z", "path": "."},
      '{"total_count": 1000}', 1000, 40))
case("_sum_search_files", **common({"pattern": "a b"}, '{"total_count":7}', 7, 3))
case("_sum_search_files", **common({"pattern": "q"}, '{"total_count":999}', 999, 9))

# ── _sum_browser (seam _result_failure_suffix x1) ─────────────────────────
case("_sum_browser", **common({"url": "https://example.com"}, "ok", 12345, 50, name="browser",
      io={"_result_failure_suffix": ""}))
case("_sum_browser", **common({"ref": "abc123"}, "ok", 99, 2, name="browser",
      io={"_result_failure_suffix": ""}))
case("_sum_browser", **common({}, "ok", 0, 0, name="browser",
      io={"_result_failure_suffix": ""}))
case("_sum_browser", **common({"url": "https://x.io"}, "err", 10, 1, name="browser",
      io={"_result_failure_suffix": " FAILED: blocked"}))
case("_sum_browser", **common({"url": "https://big.io"}, "ok", 1234567, 3, name="browser",
      io={"_result_failure_suffix": ""}))
case("_sum_browser", **common({"ref": "r"}, "err", 5, 1, name="other",
      io={"_result_failure_suffix": " FAILED"}))

# ── _sum_delegate_task (seams _str_arg x1, _result_failure_suffix x1) ─────
case("_sum_delegate_task", **common({"goal": "do the thing"}, "ok", 500, 5,
      io={"_str_arg": "do the thing", "_result_failure_suffix": ""}))
case("_sum_delegate_task", **common({"goal": "G" * 80}, "ok", 900, 9,
      io={"_str_arg": "G" * 80, "_result_failure_suffix": ""}))
case("_sum_delegate_task", **common({"goal": "failing"}, "err", 10, 1,
      io={"_str_arg": "failing", "_result_failure_suffix": " FAILED: timeout"}))
case("_sum_delegate_task", **common({"goal": ""}, "ok", 0, 0,
      io={"_str_arg": "", "_result_failure_suffix": ""}))
case("_sum_delegate_task", **common({"goal": "y" * 60}, "ok", 1234, 2,
      io={"_str_arg": "y" * 60, "_result_failure_suffix": ""}))
case("_sum_delegate_task", **common({"goal": "z" * 61}, "ok", 4321, 3,
      io={"_str_arg": "z" * 61, "_result_failure_suffix": ""}))

# ── _sum_execute_code (seam _str_arg x1) ──────────────────────────────────
case("_sum_execute_code", **common({"code": "print(1)"}, "ok", 8, 1,
      io={"_str_arg": "print(1)"}))
case("_sum_execute_code", **common({"code": "a\nb\nc\n" + "z" * 70}, "ok", 90, 4,
      io={"_str_arg": "a\nb\nc\n" + "z" * 70}))
case("_sum_execute_code", **common({"code": "c" * 60}, "ok", 60, 1,
      io={"_str_arg": "c" * 60}))
case("_sum_execute_code", **common({"code": ""}, "ok", 0, 0,
      io={"_str_arg": ""}))
case("_sum_execute_code", **common({"code": "x\ny\nz"}, "ok", 5, 3,
      io={"_str_arg": "x\ny\nz"}))
case("_sum_execute_code", **common({"code": "d" * 61}, "ok", 61, 1,
      io={"_str_arg": "d" * 61}))

# ── _sum_web_extract (seam _result_failure_suffix x1) ─────────────────────
case("_sum_web_extract", **common({"urls": ["https://a.io"]}, "ok", 100, 1,
      io={"_result_failure_suffix": ""}))
case("_sum_web_extract", **common({"urls": [{"url": "https://b.io"}]}, "ok", 200, 1,
      io={"_result_failure_suffix": ""}))
case("_sum_web_extract", **common({"urls": [{"href": "https://c.io"}]}, "ok", 300, 1,
      io={"_result_failure_suffix": ""}))
case("_sum_web_extract", **common({"urls": ["https://a", "https://b", "https://c"]}, "ok", 400, 3,
      io={"_result_failure_suffix": ""}))
case("_sum_web_extract", **common({"urls": []}, "ok", 0, 0,
      io={"_result_failure_suffix": ""}))
case("_sum_web_extract", **common({"urls": ["https://a"]}, "err", 10, 1,
      io={"_result_failure_suffix": " FAILED: rate limit"}))

# ── _sum_skills_list (seams _str_arg, _json_dict, _result_failure_suffix) ──
case("_sum_skills_list", **common({"category": "dev"}, "ok", 50, 1,
      io={"_str_arg": "dev", "_json_dict": {"count": 7}, "_result_failure_suffix": ""}))
case("_sum_skills_list", **common({}, "ok", 0, 0,
      io={"_str_arg": "", "_json_dict": {"count": 2}, "_result_failure_suffix": ""}))
case("_sum_skills_list", **common({}, "ok", 0, 0,
      io={"_str_arg": "", "_json_dict": {"count": True}, "_result_failure_suffix": ""}))
case("_sum_skills_list", **common({}, "ok", 0, 0,
      io={"_str_arg": "", "_json_dict": {}, "_result_failure_suffix": ""}))
case("_sum_skills_list", **common({"category": "ops"}, "err", 3, 0,
      io={"_str_arg": "ops", "_json_dict": {"count": 1}, "_result_failure_suffix": " FAILED: x"}))
case("_sum_skills_list", **common({}, "ok", 1, 0,
      io={"_str_arg": "", "_json_dict": [], "_result_failure_suffix": ""}))

# ── _sum_skill_manage ─────────────────────────────────────────────────────
case("_sum_skill_manage", **common({"action": "add", "name": "alpha"}, "ok", 10, 1,
      io={"_str_arg": ["add", "alpha"], "_result_failure_suffix": ""}))
case("_sum_skill_manage", **common({"operations": [{"action": "add", "name": "a"},
      {"action": "rm", "name": "b"}]}, "ok", 20, 2,
      io={"_str_arg": ["add", "a", "rm", "b"], "_result_failure_suffix": ""}))
case("_sum_skill_manage", **common({"operations": [
      {"action": "a%d" % i, "name": "n%d" % i} for i in range(5)]}, "ok", 50, 5,
      io={"_str_arg": [v for i in range(5) for v in ("a%d" % i, "n%d" % i)],
          "_result_failure_suffix": ""}))
case("_sum_skill_manage", **common({"operations": [{"action": "add", "name": "a"},
      "junk", {"action": "rm", "name": "b"}]}, "ok", 30, 3,
      io={"_str_arg": ["add", "a", "rm", "b"], "_result_failure_suffix": ""}))
case("_sum_skill_manage", **common({"action": "add", "name": "x"}, "err", 5, 0,
      io={"_str_arg": ["add", "x"], "_result_failure_suffix": " FAILED: denied"}))
case("_sum_skill_manage", **common({"operations": [], "action": "list", "name": "?"}, "ok", 0, 0,
      io={"_str_arg": ["list", "?"], "_result_failure_suffix": ""}))

# ── _sum_cronjob_manage ───────────────────────────────────────────────────
case("_sum_cronjob_manage", **common({"action": "run"}, "ok", 1, 1,
      io={"_json_dict": {"error": "oops"},
          "_payload_failure_suffix": " FAILED: oops"}))
case("_sum_cronjob_manage", **common({"action": "run"}, "ok", 1, 1,
      io={"_json_dict": {"job": {"execution_error": "boom"}},
          "_payload_failure_suffix": "", "_failure_suffix": " FAILED: boom"}))
case("_sum_cronjob_manage", **common({"action": "run"}, "ok", 1, 1,
      io={"_json_dict": {"job": {"execution_skipped": "idle"}},
          "_payload_failure_suffix": "", "_failure_suffix": " SKIPPED: idle"}))
case("_sum_cronjob_manage", **common({"action": "run"}, "ok", 1, 1,
      io={"_json_dict": {"job": {"execution_success": False}},
          "_payload_failure_suffix": "", "_failure_suffix": " FAILED"}))
case("_sum_cronjob_manage", **common({"action": "create"}, "ok", 1, 1,
      io={"_json_dict": {"job": {"execution_success": True}},
          "_payload_failure_suffix": ""}))
case("_sum_cronjob_manage", **common({"action": "run"}, "ok", 1, 1,
      io={"_json_dict": {"job": {"execution_skipped": None, "execution_success": False}},
          "_payload_failure_suffix": "", "_failure_suffix": " FAILED"}))

# ── _sum_process_manage ───────────────────────────────────────────────────
case("_sum_process_manage", **common({"action": "poll", "session_id": "s1"}, "ok", 1, 1,
      io={"_json_dict": {"error": "gone"}, "_payload_failure_suffix": " FAILED: gone"}))
case("_sum_process_manage", **common({"action": "poll", "session_id": "s2"}, "ok", 1, 1,
      io={"_json_dict": {"exit_code": 2}, "_payload_failure_suffix": "",
          "_failure_suffix": " FAILED: exit code 2"}))
case("_sum_process_manage", **common({"action": "poll", "session_id": "s3"}, "ok", 1, 1,
      io={"_json_dict": {"exit_code": 0}, "_payload_failure_suffix": ""}))
case("_sum_process_manage", **common({"action": "poll", "session_id": "s4"}, "ok", 1, 1,
      io={"_json_dict": {"exit_code": 3, "completion_reason": "killed"},
          "_payload_failure_suffix": ""}))
case("_sum_process_manage", **common({"action": "poll", "session_id": "s5"}, "ok", 1, 1,
      io={"_json_dict": {"exit_code": True}, "_payload_failure_suffix": ""}))
case("_sum_process_manage", **common({"action": "start", "session_id": "s6"}, "ok", 1, 1,
      io={"_json_dict": {"running": True}, "_payload_failure_suffix": ""}))

# ── _summarize_refused_tool_result ────────────────────────────────────────
case("_summarize_refused_tool_result", tool_name="terminal", args={"command": "rm -rf /"},
     content="", io={"_json_dict": {"status": "pending_approval"}})
case("_summarize_refused_tool_result", tool_name="write_file", args={"path": "/etc/x"},
     content="", io={"_json_dict": {"status": "blocked", "error": "sensitive path"}})
case("_summarize_refused_tool_result", tool_name="write_file", args={"path": "/p"},
     content="", io={"_json_dict": {"error": "BLOCKED by policy"}})
case("_summarize_refused_tool_result", tool_name="terminal", args={"command": "x"},
     content="", io={"_json_dict": {"error": "BLOCKED: NOT consented by user"}})
case("_summarize_refused_tool_result", tool_name="terminal", args={"command": "x"},
     content="", io={"_json_dict": {"status": "ok"}})
case("_summarize_refused_tool_result", tool_name="terminal", args={"command": "C" * 80},
     content="", io={"_json_dict": {"status": "pending_approval"}})

# ── _bullets (no seams) ───────────────────────────────────────────────────
case("_bullets", items=["a", "b", "a", " c ", ""])
case("_bullets", items=["  ", ""], limit=8)
case("_bullets", items=["x", "y", "z"], limit=2)
case("_bullets", items=[], limit=8)
case("_bullets", items=[" one ", "two", " one", "three"], limit=3)
case("_bullets", items=["alpha", "beta", "gamma", "delta"], limit=0)

# ── _tool_result_parts (no seams) ─────────────────────────────────────────
case("_tool_result_parts", content="plain text")
case("_tool_result_parts", content={"_multimodal": True, "content": [{"type": "text"}]})
case("_tool_result_parts", content={"_multimodal": False, "content": [1]})
case("_tool_result_parts", content={"content": "no envelope"})
case("_tool_result_parts", content=[1, 2, 3])
case("_tool_result_parts", content=None)

# ── _summary_part_text ────────────────────────────────────────────────────
case("_summary_part_text", part="just a string", io={})
case("_summary_part_text", part={"type": "text", "text": "hello"}, io={})
case("_summary_part_text", part={"type": "image_url"},
     io={"_image_part_label": "[image: https://i.io]"})
case("_summary_part_text", part={"type": "input_image"},
     io={"_image_part_label": "[image]"})
case("_summary_part_text", part={"type": "audio"}, io={})
case("_summary_part_text", part={}, io={})

# ── raise fixtures: exercise the FULL builtin exception hierarchy ────────
def exc(kind, msg="boom"):
    return {"__raise__": kind, "message": msg}

# base-only kinds (propagate past every `except Exception`)
case("_sum_browser", **common({"url": "https://x.io"}, "ok", 10, 1, name="browser",
      io={"_result_failure_suffix": exc("SystemExit", "bye")}))
case("_sum_terminal", **common({"command": "ls"}, "ok", 2, 1,
      io={"_str_arg": exc("KeyboardInterrupt", "stop")}))
case("_sum_skills_list", **common({}, "ok", 0, 0,
      io={"_str_arg": "", "_json_dict": exc("GeneratorExit", "gen")}))
case("_sum_cronjob_manage", **common({"action": "run"}, "ok", 1, 1,
      io={"_json_dict": {}, "_payload_failure_suffix": exc("CancelledError", "cancelled")}))
# Exception descendants (OSError family, ImportError family, generic)
case("_sum_browser", **common({"url": "https://x.io"}, "ok", 10, 1, name="browser",
      io={"_result_failure_suffix": exc("FileNotFoundError", "missing")}))
case("_summary_part_text", part={"type": "image_url"},
     io={"_image_part_label": exc("ModuleNotFoundError", "no module")})
case("_sum_browser", **common({"url": "https://x.io"}, "ok", 10, 1, name="browser",
      io={"_result_failure_suffix": exc("ValueError", "bad value")}))
case("_sum_process_manage", **common({"action": "poll", "session_id": "s"}, "ok", 1, 1,
      io={"_json_dict": {}, "_payload_failure_suffix": exc("PermissionError", "denied")}))

OUT.parent.mkdir(parents=True, exist_ok=True)
with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    for c in cases:
        f.write(json.dumps(c, ensure_ascii=False) + "\n")

# verify >=5 per function
from collections import Counter
cnt = Counter(c["func"] for c in cases)
assert len(FUNCS := set(cnt)) == 15, cnt
assert all(cnt[f] >= 5 for f in cnt), cnt
print("wrote %d cases across %d functions -> %s" % (len(cases), len(cnt), OUT))
print("per-function counts:", dict(sorted(cnt.items())))
