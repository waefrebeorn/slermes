#!/usr/bin/env python3
"""sta_oracle_display.py -- diffs the C agent/display.py port against the LIVE source.

Reads the harness JSONL on stdin. Expected values come from the real
agent.display module in the quarry (C:/Users/eman5/hermes-agent): the legacy
shell helpers and the new pure formatting ports are both called live. The
quarry import chain needs the project venv; when the current interpreter
lacks the deps we re-exec under it (stdin survives execv), mirroring
sta_oracle_credits_free_tier.py. HERMES_LANGUAGE is pinned to en so the live
i18n templates match the bundled English strings baked into the C port.
"""
import os
import sys
import json

os.environ["HERMES_LANGUAGE"] = "en"
QUARRY = "C:/Users/eman5/hermes-agent"
if os.path.isdir(os.path.join(QUARRY, "agent")):
    sys.path.insert(0, QUARRY)

try:
    from agent import display as D
except ImportError:
    _venv = os.path.join(QUARRY, ".venv", "Scripts", "python.exe")
    if not os.path.isfile(_venv):
        _venv = os.path.join(QUARRY, ".venv", "bin", "python")
    if os.path.isfile(_venv) and \
            os.path.abspath(sys.executable) != os.path.abspath(_venv):
        os.execv(_venv, [_venv, os.path.abspath(__file__)] + sys.argv[1:])
    raise

# Functions whose harness `out` is serialised JSON (array/object), unwrapped
# back into Python structures before comparison.
STRUCT_FNS = {"words", "compound", "scan_quoted", "delegate_task_goals", "hex_rgb"}
BOOL_FNS = {"echo", "is_neutral_failure"}


def to_lists(v):
    if isinstance(v, tuple):
        return [to_lists(x) for x in v]
    if isinstance(v, list):
        return [to_lists(x) for x in v]
    return v


def jargs(c):
    return json.loads(c.get("arg", "{}"))


def expected(c):
    fn = c["fn"]
    arg = c.get("arg", "")
    if fn == "oneline":
        return D._oneline(arg)
    if fn == "tail":
        return D._tail_trunc(arg, c.get("lim", 0))
    if fn == "clip":
        return D._clip(arg, c.get("n", 0))
    if fn == "words":
        return D._split_shell_words(arg)
    if fn == "pipetail":
        return D._strip_shell_pipe_tail(arg)
    if fn == "compound":
        return D._split_shell_compound(arg)
    if fn == "head":
        return D._shell_head_word(arg)
    if fn == "clean":
        return D._clean_shell_segment(arg)
    if fn == "echo":
        return bool(D._is_shell_boundary_echo(arg))
    if fn == "summary":
        return D.summarize_shell_command(arg)
    if fn == "http_url":
        return D._http_url(arg)
    if fn == "hex_rgb":
        return list(D._hex_rgb(arg))
    if fn == "fg":
        return D._fg(c["r"], c["g"], c["b"])
    if fn == "tinted_bg":
        return D._tinted_bg((c["r"], c["g"], c["b"]), c["dominant"])
    if fn == "scan_quoted":
        return [list(t) for t in D._scan_quoted(arg)]
    if fn == "verbose_tool_args_text":
        return D.verbose_tool_args_text(jargs(c), c.get("max_len", 0), bool(c.get("code_block", 0)))
    if fn == "delegate_task_goals":
        return D._delegate_task_goals(jargs(c), per_goal_len=c.get("per_goal_len", 0))
    if fn == "delegate_action_preview":
        return D._delegate_action_preview(jargs(c))
    if fn == "preview_browser_exec":
        return D._preview_browser_exec(jargs(c), c.get("max_len", 0))
    if fn == "preview_delegate_task":
        return D._preview_delegate_task(jargs(c), c.get("max_len", 0))
    if fn == "preview_process_manage":
        return D._preview_process_manage(jargs(c), c.get("max_len", 0))
    if fn == "preview_todo_list":
        return D._preview_todo_list(jargs(c), c.get("max_len", 0))
    if fn == "preview_read_file":
        return D._preview_read_file(jargs(c), c.get("max_len", 0))
    if fn == "preview_memory":
        return D._preview_memory(jargs(c), c.get("max_len", 0))
    if fn == "preview_send_message":
        return D._preview_send_message(jargs(c), c.get("max_len", 0))
    if fn == "preview_skill_view":
        return D._preview_skill_view(jargs(c), c.get("max_len", 0))
    if fn == "preview_clarify":
        return D._preview_clarify(jargs(c), c.get("max_len", 0))
    if fn == "primary_arg_preview":
        return D._primary_arg_preview(c["tool"], jargs(c), c.get("max_len", 0))
    if fn == "tool_verb":
        return D._tool_verb(arg)
    if fn == "bridge_generating_phrase":
        saved = D._friendly_tool_labels
        D._friendly_tool_labels = bool(c.get("friendly", 1))
        try:
            return D.bridge_generating_phrase(arg)
        finally:
            D._friendly_tool_labels = saved
    if fn == "degraded_suffix":
        return D._degraded_suffix(jargs(c))
    if fn == "is_neutral_failure":
        return bool(D.is_neutral_failure(c["tool"], json.loads(arg)))
    if fn == "domain":
        return D._domain(arg)
    if fn == "cute_trunc":
        D.set_tool_preview_max_len(c.get("limit", 0))
        try:
            return D._cute_trunc(arg)
        finally:
            D.set_tool_preview_max_len(0)
    if fn == "cute_path":
        D.set_tool_preview_max_len(c.get("limit", 0))
        try:
            return D._cute_path(arg)
        finally:
            D.set_tool_preview_max_len(0)
    return None

cases = mismatches = shown = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        c = json.loads(line)
    except json.JSONDecodeError:
        continue
    fn = c.get("fn")
    if fn is None:
        continue
    cases += 1
    try:
        want = expected(c)
    except Exception:
        want = None  # live raised (e.g. invalid hex) -> C models this as NULL

    got = c.get("out")
    if fn in STRUCT_FNS:
        if isinstance(got, str):
            try:
                got = json.loads(got)
            except json.JSONDecodeError:
                pass
        want = to_lists(want)
    elif fn in BOOL_FNS:
        got = bool(got)
        want = bool(want)

    if got != want:
        mismatches += 1
        if shown < 12:
            print(f"MISMATCH {fn} {c.get('arg', '')!r}: got {got!r} want {want!r}")
            shown += 1

print(f"ORACLE display: {cases} cases, {mismatches} mismatches")
raise SystemExit(1 if mismatches else 0)
