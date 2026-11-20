#!/usr/bin/env python3
"""Generate tests/fixtures/file_tools_flow2.jsonl for the file_tools_flow2
FLOW oracle (tools/file_tools.py UNCLAIMED remainder). >=5 cases per ported
function (11 funcs), exercising decision branches, ordered short-circuits, the
FULL builtin exception hierarchy, BaseException passthrough and fail-closed
missing scripts.

Script semantics match the harness/oracle (FAIL-CLOSED): a MISSING script is a
RuntimeError raise spec; array = per-call queue (last repeats, empty -> None);
single value = constant; scripted null = None; {"__raise__":cls,"msg":...} =
exception. ASCII / POSIX-lexical domain; integral JSON numbers.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "file_tools_flow2.jsonl")
CASES = []


def R(cls, msg="boom"):
    return {"__raise__": cls, "msg": msg}


def add(func, inp=None, seams=None):
    d = dict(inp or {})
    d["seams"] = seams or {}
    CASES.append({"func": func, "in": d})


# ── apply_char_budget ──
add("apply_char_budget", {"result": {"content": "old"}, "content": "a\nb\nc", "offset": 1,
                          "total_lines": 10, "max_chars": 100000},
    {"truncate_to_char_budget": [["a\nb\nc", 2, None]]})
add("apply_char_budget", {"result": {}, "content": "a\nb", "offset": 5,
                          "total_lines": "unknown", "max_chars": 100000},
    {"truncate_to_char_budget": [["a\nb", 2, None]]})
add("apply_char_budget", {"result": {"x": 1}, "content": "abcdef", "offset": 1,
                          "total_lines": 1, "max_chars": 3},
    {"truncate_to_char_budget": [["abcdef", 1, None]]})  # first line clamped
add("apply_char_budget", {"result": {}, "content": "x", "offset": 3,
                          "total_lines": 0, "max_chars": 5},
    {"truncate_to_char_budget": [["x", 0, None]]})
add("apply_char_budget", {"result": {}, "content": "x", "offset": 1,
                          "total_lines": 4, "max_chars": 5},
    {"truncate_to_char_budget": R("ValueError", "trunc failed")})
add("apply_char_budget", {"result": {}, "content": "x", "offset": 1,
                          "total_lines": 4, "max_chars": 5}, {})  # fail-closed

# ── resolved_match_path (except OSError/ValueError/RuntimeError) ──
add("resolved_match_path", {"path": "rel/file.txt", "task_id": "abc123"},
    {"resolve_path_for_task": "/task/cwd/rel/file.txt"})
add("resolved_match_path", {"path": "rel/file.txt", "task_id": "abc123"},
    {"resolve_path_for_task": R("FileNotFoundError", "gone")})
add("resolved_match_path", {"path": "/abs/p", "task_id": "t"},
    {"resolve_path_for_task": R("ValueError", "bad path")})
add("resolved_match_path", {"path": "p", "task_id": "t"},
    {"resolve_path_for_task": R("RuntimeError", "rt")})
add("resolved_match_path", {"path": "p", "task_id": "t"},
    {"resolve_path_for_task": R("TypeError", "type")})  # not caught
add("resolved_match_path", {"path": "p", "task_id": "t"},
    {"resolve_path_for_task": R("SystemExit", "bye")})  # BaseException
add("resolved_match_path", {"path": "p", "task_id": "t"}, {})  # fail-closed -> caught

# ── resolve_or_none (except Exception) ──
add("resolve_or_none", {"filepath": "a.txt", "task_id": "t"},
    {"resolve_path_for_task": "/task/a.txt"})
add("resolve_or_none", {"filepath": "a.txt", "task_id": "t"},
    {"resolve_path_for_task": None})
add("resolve_or_none", {"filepath": "a.txt", "task_id": "t"},
    {"resolve_path_for_task": R("OSError", "io")})
add("resolve_or_none", {"filepath": "a.txt", "task_id": "t"},
    {"resolve_path_for_task": R("ValueError", "v")})
add("resolve_or_none", {"filepath": "a.txt", "task_id": "t", "entry": True},
    {"resolve_entry_for_task": "/task/link"})
add("resolve_or_none", {"filepath": "a.txt", "task_id": "t", "entry": True},
    {"resolve_entry_for_task": R("SystemExit", "bye")})  # BaseException passes
add("resolve_or_none", {"filepath": "a.txt", "task_id": "t"}, {})  # fail-closed -> None

# ── write_precheck_error (ordered guards, `or` short-circuit) ──
CLEAN = {"check_sensitive_path": None, "check_cross_profile_path": None,
         "check_binary_document_write": None, "check_protected_instruction_write": None,
         "check_approval_required_write": None}
add("write_precheck_error", {"paths": ["a.txt"], "content_paths": [], "task_id": "t"}, dict(CLEAN))
add("write_precheck_error", {"paths": ["a.txt", "b.txt"], "content_paths": [], "task_id": "t"},
    {**CLEAN, "check_sensitive_path": [None, "sensitive: b.txt"]})
add("write_precheck_error", {"paths": ["a.txt"], "content_paths": [], "task_id": "t",
                             "cross_profile": True},
    {"check_sensitive_path": None, "check_protected_instruction_write": None,
     "check_approval_required_write": None})  # no cross_profile seam needed
add("write_precheck_error", {"paths": ["a.txt"], "content_paths": ["img.png"], "task_id": "t"},
    {**CLEAN, "check_binary_document_write": "binary document write denied"})
add("write_precheck_error", {"paths": ["a.txt"], "content_paths": [], "task_id": "t"},
    {**CLEAN, "check_protected_instruction_write": "protected instruction"})
add("write_precheck_error", {"paths": ["a.txt"], "content_paths": [], "task_id": "t"},
    {**CLEAN, "check_approval_required_write": "approval required"})
add("write_precheck_error", {"paths": ["a.txt"], "content_paths": [], "task_id": "t"},
    {**CLEAN, "check_sensitive_path": R("PermissionError", "denied")})
add("write_precheck_error", {"paths": ["a.txt"], "content_paths": [], "task_id": "t"}, {})  # fail-closed

# ── edit_warnings ──
NOWARN = {"file_state.check_stale": None, "check_file_staleness": None,
          "path_resolution_warning": None}
add("edit_warnings", {"paths": ["a.txt"], "path_to_resolved": {}, "task_id": "t"}, dict(NOWARN))
add("edit_warnings", {"paths": ["a.txt"], "path_to_resolved": {"a.txt": "/task/a.txt"}, "task_id": "t"},
    {**NOWARN, "file_state.check_stale": "stale (other agent)"})
add("edit_warnings", {"paths": ["a.txt"], "path_to_resolved": {"a.txt": "/task/a.txt"}, "task_id": "t"},
    {**NOWARN, "check_file_staleness": "stale on disk"})
add("edit_warnings", {"paths": ["a.txt"], "path_to_resolved": {"a.txt": "/task/a.txt"}, "task_id": "t"},
    {**NOWARN, "path_resolution_warning": "resolves outside cwd"})
add("edit_warnings", {"paths": ["a.txt", "b.txt"], "path_to_resolved": {"a.txt": "/task/a.txt", "b.txt": "/task/b.txt"}, "task_id": "t"},
    {"file_state.check_stale": [None, "b stale"], "check_file_staleness": None,
     "path_resolution_warning": None})
add("edit_warnings", {"paths": ["a.txt"], "path_to_resolved": {"a.txt": None}, "task_id": "t"},
    dict(NOWARN))  # present-null r -> falsy
add("edit_warnings", {"paths": ["a.txt"], "path_to_resolved": {"a.txt": "/task/a.txt"}, "task_id": "t"},
    {**NOWARN, "check_file_staleness": R("RuntimeError", "stat failed")})

# ── note_edited ──
NOTE = {"mark_verification_stale": None, "update_read_timestamp": None,
        "file_state.note_write": None}
add("note_edited", {"task_id": "t", "paths": ["a.txt"], "path_to_resolved": {"a.txt": "/task/a.txt"}}, dict(NOTE))
add("note_edited", {"task_id": "t", "paths": ["a.txt", "b.txt"],
                    "path_to_resolved": {"a.txt": "/task/a.txt"}}, dict(NOTE))
add("note_edited", {"task_id": "t", "paths": ["a.txt"], "path_to_resolved": {"a.txt": "/task/a.txt"},
                    "session_id": "sess-1"}, dict(NOTE))
add("note_edited", {"task_id": "t", "paths": ["a.txt"], "path_to_resolved": {"a.txt": None},
                    "session_id": None}, dict(NOTE))
add("note_edited", {"task_id": "t", "paths": ["a.txt"], "path_to_resolved": {"a.txt": "/task/a.txt"}},
    {**NOTE, "mark_verification_stale": R("RuntimeError", "mark failed")})
add("note_edited", {"task_id": "t", "paths": ["a.txt"], "path_to_resolved": {"a.txt": "/task/a.txt"}},
    {**NOTE, "file_state.note_write": R("OSError", "note failed")})
add("note_edited", {"task_id": "t", "paths": ["a.txt"], "path_to_resolved": {}}, {})  # fail-closed

# ── whole_file_rewrite_hint ──
OLD = "\n".join("L%04d:" % i + "x" * 70 for i in range(270))
NEW_HIGH = "\n".join(("L%04d:" % i + "x" * 70) if i % 26 else ("C%04d:" % i + "y" * 70)
                     for i in range(270))
NEW_LOW = "\n".join("D%04d:" % i + "y" * 70 for i in range(270))
add("whole_file_rewrite_hint", {"task_id": "t", "resolved": "/task/a.txt", "new_content": "short"},
    {})  # size guard, no seam
add("whole_file_rewrite_hint", {"task_id": "t", "resolved": None, "new_content": NEW_HIGH}, {})
add("whole_file_rewrite_hint", {"task_id": "t", "resolved": "/task/a.txt", "new_content": NEW_HIGH},
    {"read_file_raw": {"content": OLD, "error": None}})
add("whole_file_rewrite_hint", {"task_id": "t", "resolved": "/task/a.txt", "new_content": NEW_HIGH},
    {"read_file_raw": {"content": OLD, "error": "read failed"}})
add("whole_file_rewrite_hint", {"task_id": "t", "resolved": "/task/a.txt", "new_content": NEW_HIGH},
    {"read_file_raw": {"content": None, "error": None}})
add("whole_file_rewrite_hint", {"task_id": "t", "resolved": "/task/a.txt", "new_content": NEW_HIGH},
    {"read_file_raw": {"content": OLD, "error": None}})  # duplicate (queue-friendly)
add("whole_file_rewrite_hint", {"task_id": "t", "resolved": "/task/a.txt", "new_content": NEW_LOW},
    {"read_file_raw": {"content": OLD, "error": None}})  # low ratio -> None
add("whole_file_rewrite_hint", {"task_id": "t", "resolved": "/task/a.txt", "new_content": NEW_HIGH},
    {"read_file_raw": R("OSError", "raw failed")})
add("whole_file_rewrite_hint", {"task_id": "t", "resolved": "/task/a.txt", "new_content": NEW_HIGH},
    {})  # fail-closed

# ── collect_v4a_header_paths ──
TRAV = "has_traversal_component"
add("collect_v4a_header_paths", {"patch": "*** Update File: src/a.py\n@@\n-old\n+new\n"},
    {TRAV: False})
add("collect_v4a_header_paths",
    {"patch": "*** Add File: src/new.py\n*** Delete File: src/old.py\n"}, {TRAV: False})
add("collect_v4a_header_paths", {"patch": "*** Move File: src/a.py -> src/b.py\n"}, {TRAV: False})
add("collect_v4a_header_paths",
    {"patch": "*** Update File: a.py\n*** Add File: b.py\n*** Delete File: c.py\n*** Move File: d.py -> e.py\n"},
    {TRAV: False})
add("collect_v4a_header_paths", {"patch": "*** Update File: ../etc/passwd\n"}, {TRAV: True})
add("collect_v4a_header_paths", {"patch": "*** Move File: a.py -> ../secret\n"}, {TRAV: True})
add("collect_v4a_header_paths", {"patch": "*** Update File: a.py\n"},
    {TRAV: R("RuntimeError", "sec failed")})
add("collect_v4a_header_paths", {"patch": "*** Update File: a.py\n"}, {})  # fail-closed

# ── owned ──
add("owned", {"owner": "sess-1", "call_kwargs": {"path": "a.txt"}},
    {"bind_owner_enter": None, "owned_call": "ok", "bind_owner_exit": None})
add("owned", {"owner": "sess-1", "call_kwargs": {}},
    {"bind_owner_enter": None, "owned_call": {"status": "done"}, "bind_owner_exit": None})
add("owned", {"owner": "sess-1", "call_kwargs": {"path": "a.txt"}},
    {"bind_owner_enter": None, "owned_call": R("ValueError", "fn failed"), "bind_owner_exit": None})
add("owned", {"owner": "sess-1", "call_kwargs": {}},
    {"bind_owner_enter": R("RuntimeError", "bind failed")})
add("owned", {"owner": "sess-1", "call_kwargs": {"path": "a.txt", "replace_all": True}},
    {"bind_owner_enter": None, "owned_call": None, "bind_owner_exit": None})
add("owned", {"owner": "sess-1", "call_kwargs": {}}, {})  # fail-closed

# ── read_device_base ──
add("read_device_base", {"path": "/abs/dev", "task_id": "t"},
    {"path_expanduser": "/abs/dev"})
add("read_device_base", {"path": "rel/path", "task_id": "t"},
    {"path_expanduser": "rel/path", "resolve_base_dir": "/task/cwd"})
add("read_device_base", {"path": "rel/path", "task_id": "t"},
    {"path_expanduser": R("ValueError", "bad"), "hosted_scope": {"name": "sandbox"}})
add("read_device_base", {"path": "rel/path", "task_id": "t"},
    {"path_expanduser": R("ValueError", "bad"), "hosted_scope": None})  # re-raise
add("read_device_base", {"path": "rel/path", "task_id": "t"},
    {"path_expanduser": R("TypeError", "type")})  # passes through
add("read_device_base", {"path": "rel/path", "task_id": "t"},
    {"path_expanduser": "rel/path", "resolve_base_dir": R("OSError", "io")})
add("read_device_base", {"path": "rel/path", "task_id": "t"}, {})  # fail-closed

# ── dedup_stub_or_block ──
K = ["a.txt", 1, 20]
add("dedup_stub_or_block", {"task_data": {"dedup_hits": {}}, "dedup_key": K, "path": "a.txt"},
    {"cap_read_tracker_data": None})
add("dedup_stub_or_block",
    {"task_data": {"dedup_hits": {"a.txt|1|20": 1}}, "dedup_key": K, "path": "a.txt"},
    {"cap_read_tracker_data": None})
add("dedup_stub_or_block",
    {"task_data": {"dedup_hits": {"a.txt|1|20": 5}}, "dedup_key": K, "path": "a.txt"},
    {"cap_read_tracker_data": None})
add("dedup_stub_or_block",
    {"task_data": {"dedup_hits": {"a.txt|5|40": 1}}, "dedup_key": ["a.txt", 5, 40], "path": "a.txt"},
    {"cap_read_tracker_data": None})
add("dedup_stub_or_block", {"task_data": {"dedup_hits": {}}, "dedup_key": ["b.txt", 1, 50], "path": "b.txt"},
    {"cap_read_tracker_data": R("RuntimeError", "cap failed")})
add("dedup_stub_or_block", {"task_data": {"dedup_hits": {}}, "dedup_key": K, "path": "a.txt"},
    {})  # fail-closed


def main():
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        for c in CASES:
            fh.write(json.dumps(c, ensure_ascii=False) + "\n")
    counts = {}
    for c in CASES:
        counts[c["func"]] = counts.get(c["func"], 0) + 1
    print("wrote %d cases to %s" % (len(CASES), OUT))
    for f in sorted(counts):
        print("  %-26s %d" % (f, counts[f]))
    assert all(v >= 5 for v in counts.values()), "every func needs >=5 cases"


if __name__ == "__main__":
    main()
