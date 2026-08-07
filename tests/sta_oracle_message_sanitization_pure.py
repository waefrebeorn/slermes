"""Oracle for agent/message_sanitization.py pure helpers."""
import json, os, sys

DEV_ROOT = os.environ.get("HERMES_DEV_ROOT") or os.path.expanduser("~/.hermes/hermes-agent")
sys.path.insert(0, DEV_ROOT)

from agent.message_sanitization import (
    _family_rule,
    matches_reasoning_echo_family,
    reasoning_echo_family,
    needs_reasoning_echo,
    deterministic_call_id,
    coalesce_tool_call_id,
)


def run(c):
    op = c.get("op")
    if op == "ms_family_rule":
        fam = c.get("family", "")
        try:
            r = _family_rule(fam)
            return "found" if r else "none"
        except KeyError:
            return "none"
    if op == "ms_matches_reasoning_echo_family":
        return "true" if matches_reasoning_echo_family(
            c.get("family", ""), c.get("provider", ""),
            c.get("model", ""), c.get("base_url", "")) else "false"
    if op == "ms_reasoning_echo_family":
        r = reasoning_echo_family(c.get("provider", ""), c.get("model", ""), c.get("base_url", ""))
        return r if r else "null"
    if op == "ms_needs_reasoning_echo":
        r = needs_reasoning_echo(c.get("provider", ""), c.get("model", ""), c.get("base_url", ""))
        return "true" if r else "false"
    if op == "ms_deterministic_call_id":
        r = deterministic_call_id(c.get("fn_name", ""), c.get("arguments", ""), c.get("index", 0))
        return r
    if op == "ms_coalesce_tool_call_id":
        r = coalesce_tool_call_id(c.get("value"))
        return r if r else ""
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
