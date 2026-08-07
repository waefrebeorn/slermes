"""Oracle for agent/context_breakdown.py renderers."""
import json, os, sys

DEV_ROOT = os.environ.get("HERMES_DEV_ROOT") or os.path.expanduser("~/.hermes/hermes-agent")
sys.path.insert(0, DEV_ROOT)

from agent.context_breakdown import (
    _bytes_to_tokens,
    render_context_grid,
    render_context_category_lines,
    render_context_details_lines,
    render_context_breakdown_lines,
)


def run(c):
    op = c.get("op")
    if op == "cb_bytes_to_tokens":
        v = c.get("value")
        r = _bytes_to_tokens(v)
        return "null" if r is None else str(r)
    if op == "cb_render_grid":
        return "\n".join(render_context_grid(c.get("payload", {})))
    if op == "cb_render_category_lines":
        return "\n".join(render_context_category_lines(c.get("payload", {})))
    if op == "cb_render_details_lines":
        return "\n".join(render_context_details_lines(c.get("details", {})))
    if op == "cb_render_lines":
        details = c.get("details")
        grid = c.get("grid", True)
        return "\n".join(render_context_breakdown_lines(c.get("payload", {}),
                                                        details=details, grid=grid))
    return None


def main():
    fixture = sys.argv[1]
    cases = json.load(open(fixture))
    for c in cases:
        r = run(c)
        if r is None:
            print("none")
        else:
            print(r)


if __name__ == "__main__":
    main()
