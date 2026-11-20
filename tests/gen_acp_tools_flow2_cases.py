#!/usr/bin/env python3
"""Generate tests/fixtures/acp_tools_flow2.jsonl (>=5 cases per function).

For each case we drive the LIVE function (via oracle_acp_tools_flow2) under a
'world' of seam scripts, capture the ordered seam trace, and store the per-name
queue of recorded seam RETURNS as the fixture's `seams`. Replaying those scripts
in the C harness and in the oracle reproduces the identical seam behaviour, so
the differential compares the port's own control flow / string assembly.
"""
import importlib.util
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("oracle_acp_tools_flow2", HERE / "oracle_acp_tools_flow2.py")
oracle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(oracle)
oracle.load_ns()

CASES = []


def base_world(**over):
    w = {
        "uuid.uuid4": "0123456789abcdef0123456789abcdef",
        "build_tool_preview": "preview text",
        "json.loads": {},
        "json.dumps": "{}",
        "JSONDecoder.raw_decode": {},
        "acp.text_block": {"type": "text"},
        "acp.tool_content": [{"type": "text"}],
        "acp.tool_diff_content": {"type": "diff"},
        "extract_edit_diff": None,
        "acp.start_tool_call": {"started": True},
        "acp.update_tool_call": {"updated": True},
        "ToolCallLocation": {"loc": True},
        "logger.debug": None,
    }
    w.update(over)
    return w


def add(func, argv, kw=None, **world_over):
    kw = kw or {}
    world = base_world(**world_over)
    # auto-derive json.loads from the first JSON-looking string argument
    if "json.loads" not in world_over:
        for a in argv:
            if isinstance(a, str):
                try:
                    world["json.loads"] = json.loads(a)
                    break
                except Exception:
                    pass
    CASES.append((func, list(argv), dict(kw), world))


def emit():
    out = []
    for func, argv, kw, world in CASES:
        r = oracle.drive(func, argv, kw, world)
        seams = {}
        for ev in r["trace"]:
            seams.setdefault(ev["ev"], []).append(ev["ret"])
        out.append({"func": func, "in": {"argv": argv, "kw": kw, "seams": seams}})
    # >=5 per func
    counts = {}
    for c in out:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    bad = {k: v for k, v in counts.items() if v < 5}
    if bad:
        raise SystemExit("too few cases: %r" % bad)
    fx = HERE / "fixtures" / "acp_tools_flow2.jsonl"
    fx.parent.mkdir(exist_ok=True)
    fx.write_text("\n".join(json.dumps(c, ensure_ascii=False, separators=(",", ":")) for c in out) + "\n", encoding="utf-8")
    print("wrote %d cases across %d funcs -> %s" % (len(out), len(counts), fx))


# ---------------- helpers ----------------
def fmt(tn, data, args=None, _as=None, **over):
    add("_format_" + tn + "_result", [_as or tn, json.dumps(data), args or {}], **over)


# ---------------- 1 simple helpers ----------------
for n in ["read_file", "terminal", "search_files", "unknown_tool", "todo", "web_search"]:
    add("get_tool_kind", [n])
for h in ["0123456789abcdef1122334455", "ffffffffffffffffffffffff", "000000000000000000000000", "abcdef0123456789abcdef01", "1234567890abcdef12345678"]:
    add("make_tool_call_id", [], **{"uuid.uuid4": h})
add("_text", ["hello"])
add("_text", [""])
add("_text", ["line1\nline2"], **{"acp.text_block": {"t": 1}, "acp.tool_content": ["x"]})
add("_text", ["a"], **{"acp.text_block": {"__raise__": "ValueError", "message": "bad block"}})
add("_text", ["b"], **{"acp.tool_content": {"__raise__": "SystemExit", "message": "die"}})
add("_arg", [{"name": "  hi  "}, "name"])
add("_arg", [{"a": "", "b": " val "}, "a", "b"], default="d")
add("_arg", [{}, "x"], default="fallback")
add("_arg", [{"n": 5}, "n"])
add("_arg", [{"t": True}, "t"])
for data, keys, dflt in [({"a": 1, "b": 2}, ["a", "b"], ""), ({"a": 0}, ["a"], "z"), ({}, ["x"], "q"),
                         ({"x": "yes"}, ["x"], ""), ({"x": None, "y": "v"}, ["x", "y"], "d")]:
    add("_first", [data] + keys, default=dflt)
for v, t, f in [(5, "{} items", "none"), (0, "{}", "fallback"), ("hi", "v={}", "x"), (True, "T={}", "f"), (None, "{}", "none")]:
    add("_fmt", [v, t, f])
for v in ["  ok  ", "", None, 5, "x"]:
    add("_nonempty", [v])
for c, w, s in [(1, "file", "s"), (2, "file", "s"), (0, "match", "es"), (True, "x", "s"), (5, "URL", "s")]:
    add("_plural", [c, w], suffix=s)
for d, p in [({"success": False, "error": "boom"}, "patch"), ({"error": "x"}, "edit"), ({"success": True}, "p"), ({}, "p"), ({"success": False}, "p")]:
    add("_failure", [d, p])

# ---------------- 2 json helpers ----------------
add("coerce_tool_args", ['{"a": 1}'])
add("coerce_tool_args", ["not json"], **{"json.loads": {"__raise__": "ValueError", "message": "bad"}})
add("coerce_tool_args", ["not json"], **{"json.loads": {"__raise__": "SystemExit", "message": "die"}})
add("coerce_tool_args", [{"already": "dict"}])
add("coerce_tool_args", [[1, 2, 3]])
add("_args_json", [{"a": 1, "b": [2, 3]}], **{"json.dumps": '{\n  "a": 1\n}'})
add("_args_json", [{"x": True}], **{"json.dumps": {"__raise__": "TypeError", "message": "unserializable"}})
add("_args_json", [{"x": 1}], **{"json.dumps": {"__raise__": "SystemExit", "message": "die"}})
add("_args_json", [[1, 2]])
add("_args_json", ["plain"])
add("_json_loads_maybe", ['{"k": 1}'])
add("_json_loads_maybe", ['garbage'], **{"json.loads": {"__raise__": "ValueError", "message": "bad"}, "JSONDecoder.raw_decode": {"k": 2}})
add("_json_loads_maybe", ["garbage"], **{"json.loads": {"__raise__": "ValueError", "message": "bad"}, "JSONDecoder.raw_decode": {"__raise__": "ValueError", "message": "bad2"}})
add("_json_loads_maybe", ["x"], **{"json.loads": {"__raise__": "SystemExit", "message": "die"}})
add("_json_loads_maybe", [None])
add("_truncate_text", ["short"])
add("_truncate_text", ["x" * 6000])
add("_truncate_text", ["abcdef", 4])
add("_truncate_text", ["abcdefghij", 0])
add("_truncate_text", ["abc", 10])
for t, l in [("no backticks", ""), ("a `b` c", "py"), ("```x```", ""), ("``a`` ``b``", ""), ("", "z")]:
    add("_fenced_text", [t, l])
for res, tn in [("Error executing tool 'x': boom", "patch"), (json.dumps({"exit_code": 2}), "terminal"),
                (json.dumps({"success": True}), "patch"), (json.dumps({"error": "bad"}), "todo"),
                (json.dumps({"success": False}), "unknown")]:
    add("_tool_result_failed", [res, tn])
for tn, a in [("read_file", {"path": "f"}), ("terminal", {}), ("patch", {}), ("unknown", {}), ("web_search", {"query": "q"})]:
    add("build_tool_title", [tn, a])

# ---------------- 3 formatters ----------------
fmt("todo", {"todos": [{"id": 1, "content": "a", "status": "completed"}], "summary": {"completed": 1, "in_progress": 0, "pending": 0}})
fmt("todo", {"todos": [{"id": 1, "content": "parent"}, {"id": 2, "parent": 1, "content": "child"}], "summary": {"cancelled": 2}})
fmt("todo", {"todos": "not-list"})
fmt("todo", {"todos": [{"content": "  trimmed  ", "status": "in_progress"}]})
fmt("todo", {"todos": [{"id": i, "content": "t%d" % i, "status": "pending"} for i in range(10)]})
fmt("read_file", {"content": "line1|line2", "total_lines": 2, "path": "a.py"}, {"path": "a.py", "offset": 1, "limit": 10})
fmt("read_file", {"error": "nope", "content": None}, {})
fmt("read_file", {"content": "x" * 6000}, {})
fmt("read_file", {"content": "ok"}, {})
fmt("read_file", {"content": "no total", "path": "b.py"}, {})
fmt("search_files", {"files": ["a.py", "b.py"], "total_count": 2}, {})
fmt("search_files", {"matches": [{"path": "a.py", "line": 3, "content": "  hello   world  "}], "total_count": 1}, {})
fmt("search_files", {"matches": [], "message": "none"}, {})
fmt("search_files", {"files": ["x"] * 25, "truncated": True}, {})
fmt("search_files", {"matches": "bad"}, {})
fmt("execute_code", {"exit_code": 0, "stdout_bytes_total": 1234, "stdout_bytes_captured": 1000, "stdout_bytes_omitted": 234, "stdout_truncated": True, "output": "done"}, {})
fmt("execute_code", {"exit_code": 2, "error": "boom"}, {})
fmt("execute_code", {"warning": "  be careful  "}, {})
fmt("execute_code", {}, {})
fmt("execute_code", {"stdout_truncated": True, "stdout_bytes_total": "x"}, {})
fmt("skill_view", {"name": "sk", "content": "# Head\n## Sub\nbody", "description": " desc ", "linked_files": {"a": [1, 2]}}, {})
fmt("skill_view", {"success": False, "error": "nope"}, {})
fmt("skill_view", {"content": "x" * 3000}, {})
fmt("skill_view", {"file": "F", "path": "P"}, {})
fmt("skill_view", {}, {})
fmt("skill_manage", {"success": True, "replacements": 2, "message": "ok"}, {"action": "patch", "name": "sk"})
fmt("skill_manage", {"success": False, "error": "bad"}, {"action": "edit"})
fmt("skill_manage", {}, {"action": "delete", "name": "sk"})
fmt("skill_manage", {"path": "/p"}, {})
fmt("skill_manage", {"replacement_count": 3}, {"action": "create"})
fmt("web_search", {"data": {"web": [{"url": "http://x", "title": "T", "description": " d "}]}}, {})
fmt("web_search", {"web": [{"url": "http://y"}]}, {})
fmt("web_search", {"web": "bad"}, {})
fmt("web_search", {"data": {"web": [{"title": "A"}, "scalar"]}}, {})
fmt("web_search", {}, {})
fmt("web_extract", {"success": False, "error": "boom"}, {})
fmt("web_extract", {"results": [{"url": "http://x", "title": "T", "error": "fail"}, {"url": "http://y", "error": "None"}]}, {})
fmt("web_extract", {"results": "bad"}, {})
fmt("web_extract", {"results": [{"error": "  err  "}]}, {})
fmt("web_extract", {}, {})
fmt("process", {"processes": [{"status": "running", "pid": 12, "session_id": "s1", "command": "ls"}]}, {"action": "list"})
fmt("process", {"success": False, "error": "boom"}, {})
fmt("process", {"status": "done", "session_id": "s", "pid": 1, "output": "o" * 6000}, {"action": "poll"})
fmt("process", {"state": "idle", "message": "m"}, {})
fmt("process", {"processes": [1, "x"]}, {})
fmt("delegate", {"results": [{"task_index": 0, "status": "completed", "model": "m", "duration_seconds": 1.5, "summary": "s", "tool_trace": [{"tool": "read_file"}]}], "total_duration_seconds": 3}, {})
fmt("delegate", {"results": "bad"}, {})
fmt("delegate", {"error": "boom"}, {})
fmt("delegate", {"results": [{"status": "failed", "error": "e"}, "scalar"]}, {})
fmt("delegate", {"results": []}, {})
fmt("session_search", {"mode": "recent", "results": [{"title": "T", "session_id": "s", "message_count": 2, "summary": "sum"}]}, {})
fmt("session_search", {"success": False, "error": "boom"}, {})
fmt("session_search", {"results": [], "message": "none"}, {})
fmt("session_search", {"query": "q", "results": [{"when": "now", "source": "cli"}]}, {})
fmt("session_search", {"results": "bad"}, {})
fmt("memory", {"success": True, "message": "saved", "entry_count": 3, "usage": "10%"}, {"action": "save", "content": "hello"})
fmt("memory", {"success": False, "error": "boom", "matches": ["a", "b"]}, {"action": "search"})
fmt("memory", {"message": "m"}, {})
fmt("memory", {"entry_count": 0}, {})
fmt("memory", {}, {"action": "list", "target": "t"})
# _format_edit_result is NOT decorated; direct wrapper signature
_BADJ = {"__raise__": "ValueError", "message": "Expecting value"}
for data, args in [({"message": "ok", "replacements": 2}, {"path": "f"}),
                   ({"success": False, "error": "bad"}, {"path": "f"}),
                   ("plain text", {}),
                   ({"files_modified": ["a", "b"]}, {}),
                   ({}, {})]:
    ow = {}
    if isinstance(data, str):
        ow = {"json.loads": _BADJ, "JSONDecoder.raw_decode": _BADJ}
    add("_format_edit_result", ["patch", json.dumps(data) if not isinstance(data, str) else data, args], **ow)
fmt("browser", {"text": "hello", "url": "http://x", "title": "T"}, {}, _as="browser_navigate")
fmt("browser", {"images": [{"url": "u", "alt": "a"}]}, {}, _as="browser_get_images")
fmt("browser", {"success": False, "error": "boom"}, {}, _as="browser_snapshot")
fmt("browser", {"content": "c"}, {}, _as="browser_vision")
fmt("browser", {}, {}, _as="browser_snapshot")
fmt("media_or_cron", {"file_path": "f", "status": "ok"}, {}, _as="image_generate")
fmt("media_or_cron", {"success": False, "error": "boom"}, {}, _as="cronjob")
fmt("media_or_cron", {"job_id": "j", "next_run": "tomorrow"}, {}, _as="cronjob")
fmt("media_or_cron", {}, {}, _as="vision_analyze")
fmt("media_or_cron", {"message": "m"}, {}, _as="image_generate")

# ---------------- 4 structured / generic / diff / completion ----------------
for key, val, kw in [("", {"a": 1, "b": 2}, {}), ("root", {"a": [1, 2]}, {"max_depth": 1}),
                     ("k", [{"content": "item"}], {}), ("k", "scalar", {}), ("k", {}, {})]:
    add("_format_structured_value", [key, val], kw)
for tn, data in [("custom", {"message": "m", "status": "ok", "extra": 3}),
                 ("custom", [{"a": 1}, "x"]),
                 ("custom", {"success": True, "content": "body", "n": 1}),
                 ("patch", {"error": "bad"}),
                 ("custom", "plain")]:
    add("_format_generic_structured_result", [tn, json.dumps(data)], fallback_to_text=True)
diffs = [
    "--- a/f.py\n+++ b/f.py\n@@ -1 +1 @@\n-old\n+new\n",
    "--- a/x\n+++ /dev/null\n@@ -1 +0,0 @@\n-gone\n",
    "--- /dev/null\n+++ b/n.py\n@@ -0,0 +1 @@\n+new\n",
    "context only\n line\n",
    "",
]
for d in diffs:
    add("_parse_unified_diff_content", [d])
for tn, data in [("read_file", {"content": "x"}), ("todo", {"todos": []}),
                 ("skill_manage", {"message": "ok"}), ("custom", {"message": "m"}),
                 ("unknown", {"x": 1})]:
    r = json.dumps(data)
    add("_build_tool_complete_content", [tn, r], function_args={}, snapshot=None)

# ---------------- 5 "more"/start/build ----------------
for items, shown, unit in [([1, 2, 3], 2, " items"), ([1], 2, ""), ([], 0, " x"), (list(range(15)), 8, ""), ([1, 2], 1, " more")]:
    add("_more", [items, shown, unit])
for args in [{"todos": [{"status": "pending", "content": "a"}]}, {"todos": []}, {},
             {"todos": [{"id": "x"}] * 10}, {"todos": "bad"}]:
    add("_start_todo", [args])
for args in [{"action": "patch", "old_string": "o", "new_string": "n"},
             {"action": "edit", "content": "c"}, {"action": "write_file", "file_content": "f"},
             {"action": "delete", "file_path": "x"}, {"action": "other"}]:
    add("_start_skill_manage", [args])
for args in [{"code": "print(1)"}, {"code": ""}, {"code": "x" * 1300},
             {"code": "y" * 1200}, {}]:
    add("_start_execute_code", [args])
for args in [{"tasks": [{"goal": "g", "role": "r"}]}, {}, {"tasks": []},
             {"tasks": [{"goal": "x"}] * 10}, {"goal": "single"}]:
    add("_start_delegate", [args])
for label, val, lim in [("Input", "hello", 500), ("Preview", "", 100), ("X", "world", 10), ("A", "5", 100), ("B", None, 100)]:
    add("_preview", [label, val, lim])
for tn, args in [("terminal", {"command": "ls"}), ("read_file", {"path": "f"}),
                 ("unknown", {"x": 1}), ("todo", {"todos": []}), ("patch", {"path": "f", "mode": "replace"})]:
    add("build_tool_start", ["tc1", tn, args])
    add("_build_tool_start", ["tc1", tn, args])
for tn, res in [("todo", json.dumps({"todos": []})), ("web_extract", json.dumps({"success": False, "error": "e"})),
                ("patch", json.dumps({"message": "ok"})), ("custom", "plain"), ("unknown", json.dumps({"a": 1}))]:
    ow = {}
    if res == "plain":
        ow = {"json.loads": _BADJ, "JSONDecoder.raw_decode": _BADJ}
    add("build_tool_complete", ["tc1", tn, res, {}], is_error=False, **ow)
for tn in ["todo", "terminal", "patch", "unknown", "read_file"]:
    add("build_tool_abandoned", ["tc1", tn])
for a in [{"path": "f.py", "offset": 3}, {"path": "f.py", "line": 4}, {"path": "f.py"},
          {"path": "", "offset": 1}, {}]:
    add("extract_locations", [a])

emit()
