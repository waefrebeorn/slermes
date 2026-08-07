"""Oracle for agent/context_compressor.py pure helpers + staticmethods.

Reads a JSON array fixture from argv[1]; each element is {"op":<fn>, ...args}.
Recomputes from the LIVE Python source and emits one JSON object per line,
matching tests/t_port_context_compressor_pure.c.
"""
import json
import os
import sys

DEV_ROOT = os.environ.get("HERMES_DEV_ROOT") or os.path.expanduser("~/.hermes/hermes-agent")
PYAGENT = os.path.join(DEV_ROOT, "agent")
if PYAGENT not in sys.path:
    sys.path.insert(0, PYAGENT)
if DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)

from agent.context_compressor import (  # noqa: E402
    _template_visible_role, _reasoning_details_text_chars,
    ContextCompressor,
)


def _emit(op, payload):
    obj = {"op": op}
    obj.update(payload)
    print(json.dumps(obj, sort_keys=True, separators=(",", ":")))


OPS = {
    "template_visible_role": lambda c: _emit("template_visible_role",
        {"value": _template_visible_role(c.get("msg"))}),
    "reasoning_details_text_chars": lambda c: _emit("reasoning_details_text_chars",
        {"value": _reasoning_details_text_chars(c.get("value"))}),
    "rolling_summary_from_marker": lambda c: _emit("rolling_summary_from_marker",
        {"value": ContextCompressor._rolling_summary_from_marker(c.get("content", ""))}),
    "render_micro_marker_content": lambda c: _emit("render_micro_marker_content",
        {"value": ContextCompressor._render_micro_marker_content(c.get("summary", ""))}),
    "merge_adjacent_user_turns": lambda c: _emit("merge_adjacent_user_turns",
        {"value": ContextCompressor._merge_adjacent_user_turns(c.get("messages", []))}),
}


def main():
    if len(sys.argv) < 2:
        sys.stderr.write(f"usage: {sys.argv[0]} <cases.json>\n")
        return 2
    with open(sys.argv[1]) as f:
        cases = json.load(f)
    for c in cases:
        op = c.get("op", "")
        fn = OPS.get(op)
        if fn is None:
            _emit("unknown", {"error": op})
        else:
            try:
                fn(c)
            except Exception as e:
                _emit(op, {"error": str(e)})


if __name__ == "__main__":
    main()
