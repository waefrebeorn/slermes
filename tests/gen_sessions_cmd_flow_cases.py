#!/usr/bin/env python3
"""Generate tests/fixtures/sessions_cmd_flow.jsonl.

Inputs only (no expected values): the runner feeds them to the C harness,
which echoes {func, in, out}; the oracle recomputes the live reference from
the same scripts. Scripted seam results live in in.io (see the port header).
Paths are POSIX strings; env.profile_dir models store.scope.profile_dir.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "sessions_cmd_flow.jsonl")

CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


HOME_IO = {"get_hermes_home": "/home/u/.hermes",
           "native_inputs_root": "/home/u/.hermes/media"}
ENV = {"profile_dir": "/h/p1"}


def store_io(**extra):
    io = dict(HOME_IO)
    io.update(extra)
    return io


# ── _any_filter_args ────────────────────────────────────────────────────────
add("_any_filter_args", args={})
add("_any_filter_args", args={"older_than": None})
add("_any_filter_args", args={"source": ""})
add("_any_filter_args", args={"min_messages": 0})
add("_any_filter_args", args={"max_tool_calls": False})
add("_any_filter_args", args={"title": "x", "source": None})
add("_any_filter_args", args={"newer_than": "5h"})
add("_any_filter_args", args={"cwd": None, "user": None, "chat_type": None})

# ── _prune_args ─────────────────────────────────────────────────────────────
add("_prune_args", filters={})
add("_prune_args", filters={"older_than_days": 90})
add("_prune_args", filters={"source": "cli", "older_than_days": 30, "title_like": "x"})
add("_prune_args", filters={"include_pinned": True, "archived": False})
add("_prune_args", filters={"older_than_days": None})
add("_prune_args", filters={"bogus_key": 1})
add("_prune_args", filters=None)
add("_prune_args", filters={"min_messages": 3, "max_cost": 1.5, "source": "telegram"})

# ── _confirm_or_exit_code ───────────────────────────────────────────────────
PROMPT = "Delete session 's1' and all its messages? [y/N] "
add("_confirm_or_exit_code", prompt=PROMPT, io={"confirm_prompt": True})
add("_confirm_or_exit_code", prompt=PROMPT, io={"confirm_prompt": False})
add("_confirm_or_exit_code", prompt=PROMPT, io={"confirm_prompt": None})
add("_confirm_or_exit_code", prompt=PROMPT, unattended=None, io={"confirm_prompt": None})
add("_confirm_or_exit_code", prompt=PROMPT, unattended="", io={"confirm_prompt": None})
add("_confirm_or_exit_code", prompt=PROMPT,
    io={"confirm_prompt": {"__raise__": "KeyboardInterrupt"}})
add("_confirm_or_exit_code", prompt=PROMPT, declined="Aborted.", io={"confirm_prompt": False})
add("_confirm_or_exit_code", prompt=PROMPT, unattended="--json", io={"confirm_prompt": None})
add("_confirm_or_exit_code", prompt=PROMPT,
    io={"confirm_prompt": {"__raise__": "RuntimeError", "msg": "boom"}})

# ── _not_found ──────────────────────────────────────────────────────────────
add("_not_found", session_id="abc")
add("_not_found", session_id="")
add("_not_found", session_id=None)
add("_not_found", session_id="x'y")
add("_not_found", session_id="会话-1")

# ── _print_empty_store ──────────────────────────────────────────────────────
add("_print_empty_store", action="stats", args={})
add("_print_empty_store", action="pinned", args={"json": True})
add("_print_empty_store", action="pinned", args={"json": False})
add("_print_empty_store", action="pinned", args={})
add("_print_empty_store", action="list", args={})
add("_print_empty_store", action="delete", args={"json": True})
add("_print_empty_store", action="pinned", args={"json": None})

# ── _delete ─────────────────────────────────────────────────────────────────
add("_delete", session_id="s1", env=ENV, io=store_io(sessions_delete=["s1"]))
add("_delete", session_id="s1", env=ENV, io=store_io(sessions_delete=[]))
add("_delete", session_id="s1", env=ENV, io=store_io(sessions_delete=None))
add("_delete", session_id="s1", env=ENV,
    io=store_io(sessions_delete={"__raise__": "SessionWriteGuarded",
                                 "session_id": "s1", "msg": "lease held"}))
add("_delete", session_id="s1", fences={"expected_ids": ["s1", "s2"]}, env=ENV,
    io=store_io(sessions_delete=["s1"]))
add("_delete", session_id="s1", env=ENV,
    io={"get_hermes_home": "/h/", "native_inputs_root": "/m", "sessions_delete": ["s1"]})
add("_delete", session_id="s1", env=ENV,
    io=store_io(native_inputs_root={"__raise__": "OSError", "msg": "no media"}))
add("_delete", session_id="s1", env=ENV,
    io=store_io(sessions_delete=["s1"],
                after_sessions_removed={"__raise__": "RuntimeError", "msg": "post"}))
add("_delete", session_id="s1", env=ENV,
    io=store_io(native_inputs_root=None, sessions_delete=["s1"]))
add("_delete", session_id="s1", env=ENV,
    io=store_io(get_hermes_home=None, sessions_delete=["s1"]))

# ── _cmd_delete ─────────────────────────────────────────────────────────────
DEL_OK = store_io(resolve_id="s1", session_get={"pinned": False}, sessions_delete=["s1"])
add("_cmd_delete", args={"session_id": "s1", "yes": True}, env=ENV,
    io=store_io(resolve_id=None))
add("_cmd_delete", args={"session_id": "s1", "yes": True}, env=ENV, io=DEL_OK)
add("_cmd_delete", args={"session_id": "s1", "yes": True}, env=ENV,
    io=store_io(resolve_id="s1", session_get={"pinned": True}, sessions_delete=["s1"]))
add("_cmd_delete", args={"session_id": "s1", "yes": False}, env=ENV,
    io=store_io(resolve_id="s1", session_get={"pinned": False},
                confirm_prompt=True, sessions_delete=["s1"]))
add("_cmd_delete", args={"session_id": "s1", "yes": False}, env=ENV,
    io=store_io(resolve_id="s1", session_get={"pinned": True}, confirm_prompt=False))
add("_cmd_delete", args={"session_id": "s1", "yes": False}, env=ENV,
    io=store_io(resolve_id="s1", session_get={"pinned": False}, confirm_prompt=None))
add("_cmd_delete", args={"session_id": "s1", "yes": False}, env=ENV,
    io=store_io(resolve_id="s1", session_get={"pinned": False},
                confirm_prompt={"__raise__": "KeyboardInterrupt"}))
add("_cmd_delete", args={"session_id": "s1", "yes": True}, env=ENV,
    io=store_io(resolve_id="s1", session_get={"pinned": False}, sessions_delete=[]))
add("_cmd_delete", args={"session_id": "s1", "yes": True}, env=ENV,
    io=store_io(resolve_id="s1", session_get={"pinned": False},
                sessions_delete={"__raise__": "SessionWriteGuarded", "session_id": "s1"}))
add("_cmd_delete", args={"session_id": "s1", "yes": True}, env=ENV,
    io=store_io(resolve_id="s1", session_get={"pinned": False},
                sessions_delete={"__raise__": "AuthorityRefusal", "reason": "unknown_execution"}))
add("_cmd_delete", args={"session_id": "s1", "yes": True}, env=ENV,
    io=store_io(resolve_id="s1", session_get={"pinned": False},
                sessions_delete={"__raise__": "AuthorityRefusal", "reason": "session_busy"}))
add("_cmd_delete", args={"session_id": "s1", "yes": True}, env=ENV,
    io=store_io(resolve_id="s1", session_get={"pinned": False},
                sessions_delete={"__raise__": "AuthorityRefusal", "reason": "ownership_conflict"}))
add("_cmd_delete", args={"session_id": "s1", "yes": False}, env=ENV,
    io=store_io(resolve_id="s1", session_get=None, confirm_prompt=True, sessions_delete=["s1"]))
add("_cmd_delete", args={"session_id": "s1"}, env=ENV,
    io=store_io(resolve_id="s1", session_get={"pinned": False}))

# ── _cmd_rename ─────────────────────────────────────────────────────────────
add("_cmd_rename", args={"session_id": "s1", "title": ["New", "Title"]},
    io={"resolve_id": "s1", "set_title": True})
add("_cmd_rename", args={"session_id": "s1", "title": ["x"]}, io={"resolve_id": None})
add("_cmd_rename", args={"session_id": "s1", "title": []}, io={"resolve_id": "s1"})
add("_cmd_rename", args={"session_id": "s1", "title": ["  ", "\t"]}, io={"resolve_id": "s1"})
add("_cmd_rename", args={"session_id": "s1", "title": ["a\nb"]}, io={"resolve_id": "s1"})
add("_cmd_rename", args={"session_id": "s1", "title": ["a", "b\rc"]}, io={"resolve_id": "s1"})
add("_cmd_rename", args={"session_id": "s1", "title": ["x"]},
    io={"resolve_id": "s1", "set_title": False})
add("_cmd_rename", args={"session_id": "s1", "title": ["x"]},
    io={"resolve_id": "s1", "set_title": {"__raise__": "ValueError", "msg": "title too long"}})
add("_cmd_rename", args={"session_id": "s1", "title": ["  x  "]},
    io={"resolve_id": "s1", "set_title": True})

# ── _cmd_pin ────────────────────────────────────────────────────────────────
add("_cmd_pin", args={"session_ids": ["s1"]}, pinning=True,
    io={"resolve_id": "s1", "set_pinned": True, "get_title": "My Title"})
add("_cmd_pin", args={"session_ids": ["s1"]}, pinning=True,
    io={"resolve_id": "s1", "set_pinned": True, "get_title": None})
add("_cmd_pin", args={"session_ids": ["s1"]}, pinning=True,
    io={"resolve_id": "s1", "set_pinned": True, "get_title": ""})
add("_cmd_pin", args={"session_ids": ["s1"]}, pinning=False,
    io={"resolve_id": "s1", "set_pinned": True, "get_title": None})
add("_cmd_pin", args={"session_ids": ["s1"]}, pinning=True, io={"resolve_id": None})
add("_cmd_pin", args={"session_ids": ["s1"]}, pinning=True,
    io={"resolve_id": "s1", "set_pinned": False})
add("_cmd_pin", args={"session_ids": ["a", "b"]}, pinning=True,
    io={"resolve_id": {"$seq": ["a", None]}, "set_pinned": True, "get_title": "T"})
add("_cmd_pin", args={"session_ids": []}, pinning=True)
add("_cmd_pin", args={"session_ids": ["a", "b"]}, pinning=True,
    io={"resolve_id": {"$seq": [None, None]}})

# ── _print_dry_run_preview ──────────────────────────────────────────────────
add("_print_dry_run_preview", candidates=[], filters={}, io={"describe_filters": "all"})
add("_print_dry_run_preview", candidates=[{"session_id": "s1", "source": "cli"}],
    filters={"source": "cli"}, io={"describe_filters": "source=cli"})
add("_print_dry_run_preview", candidates=[{"session_id": "s1", "source": None}],
    filters={}, io={"describe_filters": "all"})
add("_print_dry_run_preview", candidates=[{"session_id": "s1", "source": ""}],
    filters={}, io={"describe_filters": "all"})
add("_print_dry_run_preview",
    candidates=[{"session_id": f"s{i}", "source": "cli"} for i in range(101)],
    filters={"older_than": "90"}, io={"describe_filters": "older than 90d"})
add("_print_dry_run_preview",
    candidates=[{"session_id": f"s{i}", "source": "cli"} for i in range(100)],
    filters={}, io={"describe_filters": "all"})
add("_print_dry_run_preview", candidates=[{"session_id": "s1", "source": "cli"}],
    filters=None, io={"describe_filters": "all"})

# ── _note_pinned_skipped ────────────────────────────────────────────────────
add("_note_pinned_skipped", filters={}, action="prune", env=ENV,
    io={"count_prune_matches": 0})
add("_note_pinned_skipped", filters={}, action="prune", env=ENV,
    io={"count_prune_matches": 1})
add("_note_pinned_skipped", filters={}, action="prune", env=ENV,
    io={"count_prune_matches": 2})
add("_note_pinned_skipped", filters={}, action="archive", env=ENV,
    io={"count_prune_matches": 1})
add("_note_pinned_skipped", filters={}, action="archive", env=ENV,
    io={"count_prune_matches": 3})
add("_note_pinned_skipped", filters={"older_than_days": 90, "source": "cli", "title_like": "x"},
    action="prune", env=ENV, io={"count_prune_matches": 1})
add("_note_pinned_skipped", filters={"bogus": 1}, action="prune", env=ENV,
    io={"count_prune_matches": 1})
add("_note_pinned_skipped", filters={}, action="prune", env=ENV,
    io={"count_prune_matches": None})

# ── _output_file_in_dir ─────────────────────────────────────────────────────
add("_output_file_in_dir", output=None, default_name="x.jsonl")
add("_output_file_in_dir", output="", default_name="x.jsonl")
add("_output_file_in_dir", output="-", default_name="x.jsonl")
add("_output_file_in_dir", output="/tmp/out.jsonl", default_name="x.jsonl")
add("_output_file_in_dir", output="/tmp/dir/", default_name="x.jsonl",
    io={"makedirs": None})
add("_output_file_in_dir", output="/tmp/dir", default_name="x.jsonl",
    io={"isdir": True, "makedirs": None})
add("_output_file_in_dir", output="/tmp/dir", default_name="x.jsonl",
    io={"isdir": False})
add("_output_file_in_dir", output="/tmp/dir/", default_name="x.jsonl",
    io={"makedirs": {"__raise__": "OSError", "msg": "permission denied"}})
add("_output_file_in_dir", output="/tmp/dir/", default_name="/abs/name.jsonl",
    io={"makedirs": None})
add("_output_file_in_dir", output="/tmp/dir//", default_name="n", io={"makedirs": None})

# ── _write_output ───────────────────────────────────────────────────────────
add("_write_output", output=None, text="hello", summary="S")
add("_write_output", output="-", text="hello", summary="S")
add("_write_output", output="", text="hello", summary="S")
add("_write_output", output="/tmp/f.jsonl", text="L1\n", summary="Saved.",
    io={"file_write": None})
add("_write_output", output="/tmp/f.jsonl", text="x", summary="S",
    io={"file_write": {"__raise__": "OSError", "msg": "disk full"}})
add("_write_output", output="-", text="x", summary="S",
    io={"stdout_write": {"__raise__": "OSError", "msg": "closed"}})
add("_write_output", output=None, text="", summary="")
add("_write_output", output="/tmp/f.jsonl", text="", summary="", io={"file_write": None})

# ── _render_only ────────────────────────────────────────────────────────────
RO_ARGS = {"format": "md", "only": "user-prompts", "output": "/tmp/o.md"}
RO_IO = {"render_sessions_export": "RENDERED", "export_record_count": [1, "prompt"]}
add("_render_only", args=RO_ARGS, sessions=[{"id": "s1"}], io=RO_IO)
add("_render_only", args=dict(RO_ARGS, format="jsonl"), sessions=[{"id": "s1"}], io=RO_IO)
add("_render_only", args=RO_ARGS, sessions=[{"id": "s1"}, {"id": "s2"}],
    io={"render_sessions_export": "R", "export_record_count": [2, "prompt"]})
add("_render_only", args=RO_ARGS, sessions=[],
    io={"render_sessions_export": "", "export_record_count": [0, "session"]})
add("_render_only", args=dict(RO_ARGS, output=None), sessions=[{"id": "s1"}], io=RO_IO)
add("_render_only", args=dict(RO_ARGS, format="html"), sessions=[{"id": "s1"}], io=RO_IO)
add("_render_only", args=dict(RO_ARGS, only=None), sessions=[{"id": "s1"}],
    io={"render_sessions_export": "R", "export_record_count": [1, "prompt"]})

# ── _render_html ────────────────────────────────────────────────────────────
add("_render_html", args={"output": "/tmp/o.html"}, sessions=[{"id": "s1"}],
    io={"generate_html_export": "<html>"})
add("_render_html", args={"output": "/tmp/o.html"}, sessions=[],
    io={"generate_multi_session_html_export": "<multi>"})
add("_render_html", args={"output": "/tmp/o.html"},
    sessions=[{"id": "s1"}, {"id": "s2"}, {"id": "s3"}],
    io={"generate_multi_session_html_export": "<multi>"})
add("_render_html", args={"output": None}, sessions=[{"id": "s1"}],
    io={"generate_html_export": "<html>"})
add("_render_html", args={"output": "/tmp/o.html"}, sessions=[{"id": "s1"}],
    io={"generate_html_export": None})
add("_render_html", args={"output": "/tmp/o.html"}, sessions=None)

# ── _export_flat ────────────────────────────────────────────────────────────
add("_export_flat", kind="only", args={"format": "html", "output": "/tmp/o.md"})
add("_export_flat", kind="html", args={"format": "html", "output": None})
add("_export_flat", kind="html", args={"format": "html", "output": "-"})
add("_export_flat", kind="jsonl", args={"format": "jsonl", "output": None})
add("_export_flat", kind="jsonl", args={"format": "jsonl", "output": "-", "session_id": None},
    io={"collect": 1})
add("_export_flat", kind="jsonl", args={"format": "jsonl", "output": "-", "session_id": None},
    io={"collect": None})
add("_export_flat", kind="jsonl", args={"format": "jsonl", "output": "-", "session_id": "s1"},
    io={"collect": [{"id": "s1"}], "default_save_filename": "s1.jsonl",
        "render_jsonl": ["L1\n", "Exported 1 session to -"]})
add("_export_flat", kind="jsonl",
    args={"format": "jsonl", "output": "/tmp/dir/", "session_id": None},
    io={"collect": [{"id": "s1"}, {"id": "s2"}], "makedirs": None,
        "render_jsonl": ["L1\nL2\n", "Exported 2 sessions to /tmp/dir/hermes_sessions.jsonl"],
        "file_write": None})
add("_export_flat", kind="only",
    args={"format": "md", "only": "user-prompts", "output": "/tmp/o.md", "session_id": None},
    io={"collect": [{"id": "s1"}], "default_save_filename": "s1.md", "isdir": False,
        "render_sessions_export": "MD", "export_record_count": [1, "prompt"],
        "file_write": None})
add("_export_flat", kind="html",
    args={"format": "html", "output": "/tmp/o.html", "session_id": None},
    io={"collect": [{"id": "s1"}], "default_save_filename": "s1.html", "isdir": False,
        "generate_html_export": "<html>", "file_write": None})
add("_export_flat", kind="bogus", args={"format": "jsonl", "output": "-"})
add("_export_flat", kind="jsonl", args={"format": "jsonl", "output": "-", "session_id": None},
    io={"collect": {"__raise__": "RuntimeError", "msg": "boom"}})
add("_export_flat", kind="only",
    args={"format": "jsonl", "only": "user-prompts", "output": "-", "session_id": None},
    io={"collect": [], "render_sessions_export": "", "export_record_count": [0, "prompt"]})
add("_export_flat", kind="jsonl", args={"format": "jsonl", "output": "-", "session_id": None},
    io={"collect": True})
add("_export_flat", kind="jsonl", args={"format": "jsonl", "output": "-", "session_id": None},
    io={"collect": 1.5})

# ── _print_recovery_verdict ─────────────────────────────────────────────────
add("_print_recovery_verdict", report={"complete": True}, output="/tmp/r.db")
add("_print_recovery_verdict",
    report={"complete": False, "verified": True,
            "verification": {"table_counts": {"sessions": 1234, "messages": 56789}}},
    output="/tmp/r.db", allow_partial=True)
add("_print_recovery_verdict",
    report={"verified": True, "best_effort": True,
            "verification": {"table_counts": {"sessions": 2, "messages": 3}}},
    output="/tmp/r.db", allow_partial=True)
add("_print_recovery_verdict", report={"verified": True}, output="/tmp/r.db",
    allow_partial=False)
add("_print_recovery_verdict", report={}, output="/tmp/r.db")
add("_print_recovery_verdict", report={"verified": True}, output="/tmp/r.db",
    allow_partial=True)
add("_print_recovery_verdict",
    report={"verified": True, "verification": {"table_counts": {"sessions": None, "messages": 0}}},
    output="/tmp/r.db", allow_partial=True)
add("_print_recovery_verdict", report={"verified": True, "verification": None},
    output="/tmp/r.db", allow_partial=True)
add("_print_recovery_verdict", report={"complete": "yes"}, output="/tmp/r.db")
add("_print_recovery_verdict", report={"complete": True}, output=None)
add("_print_recovery_verdict",
    report={"verified": True, "verification": {"table_counts": {"sessions": 1, "messages": 1}}},
    output="/tmp/r.db", allow_partial=True)

# ── _repair_prompts_pin_names ───────────────────────────────────────────────
add("_repair_prompts_pin_names",
    record={"tool_pin": {"tools": [{"function": {"name": "skill_manage"}},
                                   {"function": {"name": "memory"}}]}})
add("_repair_prompts_pin_names", record={"tool_pin": ["a", "b"]})
add("_repair_prompts_pin_names", record={"tool_pin": {"tools": "notlist"}})
add("_repair_prompts_pin_names", record={"tool_pin": {"tools": [{"function": {"name": ""}}]}})
add("_repair_prompts_pin_names", record={"tool_pin": {"tools": [{"function": "notdict"}]}})
add("_repair_prompts_pin_names", record={"tool_pin": {"tools": [{"function": {}}]}})
add("_repair_prompts_pin_names", record={"tool_pin": {"tools": [42]}})
add("_repair_prompts_pin_names", record={"tool_pin": {"tools": [None]}})
add("_repair_prompts_pin_names", record={"tool_pin": None})
add("_repair_prompts_pin_names", record={"tool_pin": "str"})
add("_repair_prompts_pin_names", record={"tool_pin": {"tools": []}})
add("_repair_prompts_pin_names",
    record={"tool_pin": {"tools": ["ok", {"function": {"name": "x"}}]}})
add("_repair_prompts_pin_names", record={"tool_pin": {"tools": [{"name": "x"}]}})
add("_repair_prompts_pin_names", record={})
add("_repair_prompts_pin_names", record={"tool_pin": {"tools": [{"function": {"name": 5}}]}})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        for case in CASES:
            f.write(json.dumps(case, ensure_ascii=False) + "\n")
    counts = {}
    for case in CASES:
        counts[case["func"]] = counts.get(case["func"], 0) + 1
    print(f"wrote {len(CASES)} cases -> {OUT}")
    for fn in sorted(counts):
        print(f"  {fn}: {counts[fn]}")
    thin = [fn for fn, c in counts.items() if c < 5]
    if thin:
        raise SystemExit(f"fewer than 5 cases: {thin}")


if __name__ == "__main__":
    main()
