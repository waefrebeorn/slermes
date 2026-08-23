"""twin for display_build_tool_preview.

Traces the LIVE agent/display.py::build_tool_preview with the same rung
order as the C harness. LIVE helpers (_oneline, _truncate_preview,
summarize_shell_command, _read_file_line_label,
redact_tool_args_for_display) supply real semantics; the final preview is
folded as a V-event so output text must match byte-for-byte.

Input token format: "tool_name|max_len|args_json".
"""
import json as _json
import os, sys, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_agent_display", os.path.join(DEV, "agent", "display.py"))
_disp = ilu.module_from_spec(_spec)
sys.modules["wc_live_agent_display"] = _disp
_spec.loader.exec_module(_disp)

PRIMARY_ARGS = {
    "terminal": "command", "web_search": "query", "web_extract": "urls",
    "read_file": "path", "write_file": "path", "patch": "path",
    "search_files": "pattern", "browser_navigate": "url",
    "browser_click": "ref", "browser_type": "text",
    "image_generate": "prompt", "text_to_speech": "text",
    "vision_analyze": "question",
    "skill_view": "name", "skills_list": "category",
    "cronjob": "action",
    "execute_code": "code", "delegate_task": "goal",
    "clarify": "question", "skill_manage": "name",
}
FALLBACK_KEYS = ("query", "text", "command", "path", "name", "prompt", "code", "goal")

# rung sentinel: abort the ladder early (both sides still fold the V event)
class _Early(Exception):
    pass


def trace(token):
    parts = token.split("|", 2)
    tool = parts[0] if parts else ""
    max_len = int(parts[1]) if len(parts) > 1 and parts[1] else 80
    raw_args = parts[2] if len(parts) > 2 and parts[2] else ""
    args = _json.loads(raw_args) if raw_args else None
    res = None
    c = Cup("build_tool_preview")
    try:
        if not args:
            c.branch(True)                                   # B1
            raise _Early
        c.branch(False)
        c.branch(True)                                       # B2 parse ok
        c.branch(True)                                       # B3 object

        if tool == "delegate_task":
            c.branch(True)
            tasks = args.get("tasks")
            if tasks and isinstance(tasks, list):
                c.branch(True)
                for _ in tasks:                              # LOOP over tasks
                    c.loop()
            else:
                c.branch(False)
                goal = args.get("goal")
                if goal is None:
                    c.branch(True)
                    raise _Early
                c.branch(False)
                gp = _disp._oneline(str(goal))
                c.branch(bool(gp))
        elif tool == "process":
            c.branch(False); c.branch(True)
            action = args.get("action", "")
            sid = args.get("session_id", "")
            data = args.get("data", "")
            timeout_val = args.get("timeout")
            c.branch(bool(sid))                              # B sid
            c.branch(bool(data))                             # B data
            if timeout_val and action == "wait":             # B timeout+wait
                c.branch(True)
                c.branch(isinstance(timeout_val, (int, float)))  # B number
            else:
                c.branch(False)
        elif tool == "todo":
            c.branch(False); c.branch(False); c.branch(True)
            todos_arg = args.get("todos")
            merge = args.get("merge", False)
            if todos_arg is None:
                c.branch(True)
            elif merge:
                c.branch(False); c.branch(True)
            else:
                c.branch(False); c.branch(False)
        elif tool in ("terminal", "execute_code"):
            c.branch(False); c.branch(False); c.branch(False); c.branch(True)
            key = "code" if tool == "execute_code" else "command"
            command = args.get(key)
            if command is None or not isinstance(command, str):
                c.branch(False)
                raise _Early
            c.branch(True)
            preview = _disp.summarize_shell_command(str(command))
            c.branch(bool(preview))                          # B sum nonempty
        elif tool == "read_file":
            c.branch(False); c.branch(False); c.branch(False); c.branch(False)
            c.branch(True)
            path = args.get("path") or args.get("file") or args.get("filepath")
            if path is None or not isinstance(path, str):
                c.branch(False)
                raise _Early
            c.branch(True)
            line_label = _disp._read_file_line_label(args)
            c.branch(bool(line_label))                       # B line lbl
        elif tool == "session_search":
            for _ in range(5):
                c.branch(False)
            c.branch(True)
        elif tool == "memory":
            for _ in range(6):
                c.branch(False)
            c.branch(True)
            action = args.get("action", "")
            if action == "add":
                c.branch(True)
            elif action in ("replace", "remove"):
                c.branch(False); c.branch(True)
            else:
                c.branch(False); c.branch(False)
        elif tool == "send_message":
            for _ in range(7):
                c.branch(False)
            c.branch(True)
            msg = _disp._oneline(str(args.get("message", "")))
            c.branch(len(msg) > 20)                          # B long msg
        else:
            for _ in range(8):
                c.branch(False)
            c.branch(False)
            key = PRIMARY_ARGS.get(tool)
            if not key:
                c.branch(False)                              # B primary key
                for fb in FALLBACK_KEYS:                     # LOOP fallback keys
                    c.loop()
                    if fb in args:
                        key = fb
                        break
            else:
                c.branch(True)
            if not key or key not in args:
                c.branch(False)                              # B resolvable
                raise _Early
            c.branch(True)
            value = args[key]
            if isinstance(value, list):                      # B array
                c.branch(True); c.branch(False)
            elif isinstance(value, str):                     # B string
                c.branch(False); c.branch(True)
            else:
                c.branch(False); c.branch(False)

        res = _disp.build_tool_preview(tool, args, max_len=max_len)
    except _Early:
        pass

    v = "" if res is None else str(res)
    c.valstr(v)
    return res, c.finalize()


def run(inputs):
    cups = []
    for tok in inputs:
        _, cup = trace(tok)
        cups.append(cup)
    return cups
