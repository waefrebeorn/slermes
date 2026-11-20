#!/usr/bin/env python3
"""Generate tests/fixtures/display_flow2.jsonl.

Inputs only (no expected values): the oracle recomputes the live reference.
Scripted seam results live in in.io (see the harness header). Representation
scope: ASCII text inputs; a tool label double carries {name,emoji,text,preview};
a skin double is {"spinner": {key: [...]}}; an _out double is {"__proxy__": bool};
start() state carries running/start_time/thread; no integral-float JSON
literals / non-finite floats.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures", "display_flow2.jsonl")
CASES = []


def add(func, **inp):
    CASES.append({"func": func, "in": inp})


def script(ret=None, raise_spec=None):
    s = {"__script__": True, "ret": ret}
    if raise_spec is not None:
        s["raise"] = raise_spec
    return s


def label(name="n", emoji="e", text="t", preview="p"):
    return {"name": name, "emoji": emoji, "text": text, "preview": preview}


def exc(kind, msg="boom"):
    return {"type": kind, "message": msg}


# ── _preview_shell ───────────────────────────────────────────────────────
add("_preview_shell", key="command", args={"command": "ls -la"}, max_len=20,
    io={"sanitize_command_for_display": "SAN", "summarize_shell_command": "SUM", "_tail_trunc": "ls -la"})
add("_preview_shell", key="command", args={"command": "ls -la"}, max_len=20,
    io={"sanitize_command_for_display": "SAN", "summarize_shell_command": "SUM", "_tail_trunc": ""})
add("_preview_shell", key="command", args={}, max_len=20, io={})                      # None command
add("_preview_shell", key="code", args={"code": 5}, max_len=10,
    io={"sanitize_command_for_display": "S5", "summarize_shell_command": "SUM5", "_tail_trunc": "5"})
add("_preview_shell", key="command", args={"command": True}, max_len=10,
    io={"sanitize_command_for_display": "ST", "summarize_shell_command": "SUMT", "_tail_trunc": "True"})
add("_preview_shell", key="command", args={"command": "x"}, max_len=10,
    io={"sanitize_command_for_display": script(raise_spec=exc("ValueError", "bad sanitize"))})
add("_preview_shell", key="command", args={"command": "x"}, max_len=10,
    io={"sanitize_command_for_display": "SAN",
        "summarize_shell_command": script(raise_spec=exc("TypeError", "bad summarize"))})
add("_preview_shell", key="command", args={"command": "x"}, max_len=10,
    io={"sanitize_command_for_display": "SAN", "summarize_shell_command": "SUM",
        "_tail_trunc": script(raise_spec=exc("KeyboardInterrupt", "stop"))})
add("_preview_shell", key="code", args={"code": "print(1)"}, max_len=0,
    io={"sanitize_command_for_display": "SC", "summarize_shell_command": "PSC", "_tail_trunc": "print(1)"})

# ── _preview_bridge_call ─────────────────────────────────────────────────
add("_preview_bridge_call", tool_name="tool_call", args={"name": "x"}, max_len=30,
    io={"bridge_tool_labels": [[]], "_primary_arg_preview": "ARG"})
add("_preview_bridge_call", tool_name="tool_call", args={"name": "x"}, max_len=30,
    io={"bridge_tool_labels": [[label(text="one")]], "_tail_trunc": "one"})
add("_preview_bridge_call", tool_name="tool_search", args={"q": "x"}, max_len=30,
    io={"bridge_tool_labels": [[label(text="one"), label(text="two")]], "_tail_trunc": "one +1"})
add("_preview_bridge_call", tool_name="tool_describe", args={"n": "x"}, max_len=30,
    io={"bridge_tool_labels": [[label(text="a"), label(text="b"), label(text="c")]], "_tail_trunc": "TR"})
add("_preview_bridge_call", tool_name="tool_call", args={"name": "x"}, max_len=30,
    io={"bridge_tool_labels": [[label(text="one")]], "_tail_trunc": ""})
add("_preview_bridge_call", tool_name="tool_call", args={"name": "x"}, max_len=30,
    io={"bridge_tool_labels": script(raise_spec=exc("RuntimeError", "labels boom"))})
add("_preview_bridge_call", tool_name="tool_call", args={"name": "x"}, max_len=30,
    io={"bridge_tool_labels": [[]],
        "_primary_arg_preview": script(raise_spec=exc("SystemExit", "bye"))})
add("_preview_bridge_call", tool_name="tool_call", args={"name": "x"}, max_len=30,
    io={"bridge_tool_labels": [[label(text="one")]],
        "_tail_trunc": script(raise_spec=exc("GeneratorExit", "gen"))})

# ── _skin_spinner_list ───────────────────────────────────────────────────
add("_skin_spinner_list", key="waiting_faces", fallback=["a"], io={"_get_skin": None})
add("_skin_spinner_list", key="waiting_faces", fallback=["a"],
    io={"_get_skin": {"spinner": {"waiting_faces": ["x", "y"]}}})
add("_skin_spinner_list", key="waiting_faces", fallback=["a"],
    io={"_get_skin": {"spinner": {"waiting_faces": []}}})
add("_skin_spinner_list", key="waiting_faces", fallback=["a"],
    io={"_get_skin": {"spinner": {"other": ["z"]}}})
add("_skin_spinner_list", key="waiting_faces", fallback=["a"],
    io={"_get_skin": {"tool_emojis": {}}})
add("_skin_spinner_list", key="thinking_faces", fallback=["f"],
    io={"_get_skin": script(raise_spec=exc("RuntimeError", "skin boom"))})
add("_skin_spinner_list", key="thinking_faces", fallback=["f"],
    io={"_get_skin": script(raise_spec=exc("KeyboardInterrupt", "kbi"))})
add("_skin_spinner_list", key="thinking_faces", fallback=["f"],
    io={"_get_skin": {"spinner": ["not", "a", "dict"]}})

# ── _catalog_thinking_verbs ──────────────────────────────────────────────
add("_catalog_thinking_verbs", io={"t": "a, b ,c"})
add("_catalog_thinking_verbs", io={"t": ""})
add("_catalog_thinking_verbs", io={"t": "display.thinking_verbs"})
add("_catalog_thinking_verbs", io={"t": " , , "})
add("_catalog_thinking_verbs", io={"t": "pondering"})
add("_catalog_thinking_verbs", io={"t": script(raise_spec=exc("ValueError", "t boom"))})
add("_catalog_thinking_verbs", io={"t": script(raise_spec=exc("CancelledError", "cancelled"))})

# ── _is_patch_stdout_proxy ───────────────────────────────────────────────
add("_is_patch_stdout_proxy", state={"_out": {"__proxy__": True}},
    io={"StdoutProxy_import": True})
add("_is_patch_stdout_proxy", state={"_out": {"__proxy__": False}},
    io={"StdoutProxy_import": True})
add("_is_patch_stdout_proxy", state={"_out": {"__proxy__": True}},
    io={"StdoutProxy_import": script(raise_spec=exc("ImportError", "no proxy"))})
add("_is_patch_stdout_proxy", state={"_out": {"__proxy__": True}},
    io={"StdoutProxy_import": script(raise_spec=exc("ModuleNotFoundError", "no module"))})
add("_is_patch_stdout_proxy", state={"_out": {"__proxy__": True}},
    io={"StdoutProxy_import": script(raise_spec=exc("OSError", "import os err"))})
add("_is_patch_stdout_proxy", state={}, io={"StdoutProxy_import": True})
add("_is_patch_stdout_proxy", state={"_out": "plain"}, io={"StdoutProxy_import": True})

# ── start ────────────────────────────────────────────────────────────────
add("start", state={"running": False},
    io={"time.time": 100.5, "threading.Thread": "THR", "thread.start": None})
add("start", state={},
    io={"time.time": 0, "threading.Thread": "T2", "thread.start": None})
add("start", state={"running": True}, io={})
add("start", state={"running": 1}, io={})
add("start", state={"running": False},
    io={"time.time": script(raise_spec=exc("OSError", "clock"))})
add("start", state={"running": False},
    io={"time.time": 5,
        "threading.Thread": script(raise_spec=exc("RuntimeError", "thread boom"))})
add("start", state={"running": False},
    io={"time.time": 5, "threading.Thread": "THR",
        "thread.start": script(raise_spec=exc("KeyboardInterrupt", "start kbi"))})

# ── _clear_line_blanks ───────────────────────────────────────────────────
add("_clear_line_blanks", state={"last_line_len": 0})
add("_clear_line_blanks", state={"last_line_len": 10})
add("_clear_line_blanks", state={"last_line_len": 35})
add("_clear_line_blanks", state={"last_line_len": 40})
add("_clear_line_blanks", state={"last_line_len": 100})
add("_clear_line_blanks", state={})
add("_clear_line_blanks", state={"last_line_len": -5})


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        for c in CASES:
            f.write(json.dumps(c, ensure_ascii=False) + "\n")
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
