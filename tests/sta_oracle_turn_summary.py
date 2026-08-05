"""Differential oracle for agent/turn_summary.py.

Reads the C driver's JSON lines from stdin and compares each against the expected
value computed from LIVE Python (the parent repo's agent/turn_summary.py).
Prints `agent/turn_summary oracle: N cases, M mismatches` and one
`MISMATCH case=...` line per divergence. Exit 0 on full match.
"""

import sys
import json
import os

_PARENT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _PARENT not in sys.path:
    sys.path.insert(0, _PARENT)

from agent import turn_summary as tsmod  # noqa: E402


def main():
    raw = sys.stdin.read()
    cases = [json.loads(line) for line in raw.splitlines() if line.strip()]
    mismatches = 0

    def run(case):
        if case == "elapsed":
            return {
                "a": tsmod.format_elapsed(12.4) == "12.4s",
                "b": tsmod.format_elapsed(125.0) == "2m05s",
                "neg": tsmod.format_elapsed(-3.0) == "0.0s",
            }
        if case == "pluralize":
            return {
                "p1": tsmod._pluralize(1, "file") == "1 file",
                "p2": tsmod._pluralize(3, "files") == "3 files",
                "p3": tsmod._pluralize(1, "boxes") == "1 boxe",
                "p4": tsmod._pluralize(2, "pages") == "2 pages",
                "p5": tsmod._pluralize(1, "statuses") == "1 status",
            }
        if case == "diff":
            a, r = tsmod._count_diff_lines("--- a/x\n+++ b/x\n@@ -1,1 +1,2 @@\n-old\n+new\n+more\n")
            return {"added": a, "removed": r}
        if case == "empty_fast":
            return {"empty": tsmod.format_turn_summary(1.0, tsmod.TurnTally()) == ""}
        if case == "collector":
            col = tsmod.TurnSummaryCollector()
            diff = json.dumps({"diff": "--- a\n+++ b\n@@ -1 +1,2 @@\n-a\n+b\n+c\n"})
            col.record_tool("write_file", result=None)
            col.record_tool("patch", result=diff)
            col.record_tool("read_file")
            col.record_tool("read_file")
            col.record_tool("terminal")
            col.record_tool("terminal")
            col.record_tool("terminal")
            col.record_tool("_thinking")
            col.record_tool("unknown_tool")
            col.record_tool("write_file", is_error=True)
            return {"total": col.tally.total_tools,
                    "line": True}  # exact line compared separately below
        if case == "render_line":
            # Recompute the same collector as in "collector" case.
            col = tsmod.TurnSummaryCollector()
            diff = json.dumps({"diff": "--- a\n+++ b\n@@ -1 +1,2 @@\n-a\n+b\n+c\n"})
            col.record_tool("write_file", result=None)
            col.record_tool("patch", result=diff)
            col.record_tool("read_file")
            col.record_tool("read_file")
            col.record_tool("terminal")
            col.record_tool("terminal")
            col.record_tool("terminal")
            col.record_tool("_thinking")
            col.record_tool("unknown_tool")
            col.record_tool("write_file", is_error=True)
            return {"line": col.render(12.4)}
        if case == "many_verbs":
            col = tsmod.TurnSummaryCollector()
            for t in ["write_file", "read_file", "terminal", "execute_code", "search_files", "web_search"]:
                col.record_tool(t)
            return {"line": col.render(30.0)}
        if case == "token_flow":
            return {
                "zero": tsmod.format_token_flow(0, arrow="↓") == "",
                "small": tsmod.format_token_flow(500, arrow="↓") == "↓ 500 tok",
                "k": tsmod.format_token_flow(1200, arrow="↓") == "↓ 1.2k tok",
                "M": tsmod.format_token_flow(2500000, arrow="↓") == "↓ 2.5M tok",
            }
        return {}

    for c in cases:
        case = c["case"]
        expected = run(case)
        for k, ev in expected.items():
            if k not in c:
                mismatches += 1
                print("MISMATCH case=%s key=%s PY=%r C=MISSING" % (case, k, ev))
                continue
            cv = c[k]
            if isinstance(ev, bool):
                eq = (cv is True or cv == "true" or cv == 1) == ev
            elif isinstance(ev, int):
                try:
                    eq = (int(cv) == ev)
                except (TypeError, ValueError):
                    eq = False
            elif isinstance(ev, str):
                eq = (cv == ev)
            else:
                eq = (cv == ev)
            if not eq:
                mismatches += 1
                print("MISMATCH case=%s %s PY=%r C=%r" % (case, k, ev, cv))

    print("agent/turn_summary oracle: %d cases, %d mismatches" % (len(cases), mismatches))
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()
