#!/usr/bin/env python3
"""sta_oracle_apply_layout_tool.py — Python twin for tools/apply_layout_tool.py,
mirrored by src/tools/apply_layout_tool.c.

The real module imports tools.registry/tools.desktop_ui (heavy package graph),
so this twin mirrors apply_layout_tool's decision tree exactly (same contract
as the loops_pure precedent) with a scripted fake bridge. Fixture ops:
  {"kind":"apply","preset":...,"emit":"ok"|"false"|"raise"|"none"}
  {"kind":"schema"}
"""
import json
import sys

TIMEOUT_NOTE = "Layout apply is only available in the Hermes desktop app."

APPLY_LAYOUT_SCHEMA = {
    "name": "apply_layout",
    "description": (
        "Apply a saved layout preset to the Hermes desktop app when the user asks to "
        "rearrange the workspace — e.g. \"set up my layout for coding\", \"give me a "
        "focused view\", \"put the terminal front and center\". Built-in presets: "
        "default (chat + sidebars), focus (chat only), terminal-deck (terminal "
        "forward), quad (four zones). Plugin and user-saved presets are addressed by "
        "their id. To reveal a single pane without rearranging everything, use "
        "focus_pane instead."
    ),
    "parameters": {
        "type": "object",
        "properties": {
            "preset": {
                "type": "string",
                "description": "Layout preset id to apply (e.g. 'default', 'focus', 'terminal-deck', 'quad', or a user/plugin preset id).",
            },
        },
        "required": ["preset"],
    },
}


def tool_error(message):
    return {"error": message}


def apply_layout_tool(preset, emit_mode):
    """Mirror of tools/apply_layout_tool.py:apply_layout_tool. Returns
    (result_obj, event, payload)."""
    name = (preset or "").strip()
    if not name:
        return tool_error(
            "preset is required — a layout preset id, e.g. 'default' or 'focus'."
        ), "", None
    if emit_mode == "none":
        return tool_error(TIMEOUT_NOTE), "", None

    event = "layout.apply"
    payload = {"preset": name}
    if emit_mode == "raise":
        return tool_error(
            f"Failed to apply layout '{name}': desktop_ui bridge raised"
        ), event, payload
    if emit_mode == "false":
        return tool_error(TIMEOUT_NOTE), event, payload
    return {"success": True, "preset": name}, event, payload


def main():
    fixture = sys.argv[1]
    with open(fixture) as f:
        root = json.load(f)
    for c in root.get("ops", []):
        kind = c.get("kind")
        if kind == "apply":
            out, event, payload = apply_layout_tool(
                c.get("preset", ""), c.get("emit", "ok"))
            print(json.dumps({
                "out": out,
                "event": event,
                "payload_obj": payload if payload is not None else None,
            }, ensure_ascii=False))
        elif kind == "schema":
            print(json.dumps({"out": APPLY_LAYOUT_SCHEMA}, ensure_ascii=False))
        else:
            print(json.dumps({"out": None}))


if __name__ == "__main__":
    main()
