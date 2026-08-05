"""Differential oracle for hermes_cli/prompt_stash.py.

Reads the C driver's JSON lines from stdin and compares each against the expected
value computed from LIVE Python (the parent repo's hermes_cli/prompt_stash.py).
Prints `hermes_cli/prompt_stash oracle: N cases, M mismatches` and one
`MISMATCH case=...` line per divergence. Exit 0 on full match.
"""

import sys
import json
import os

_PARENT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _PARENT not in sys.path:
    sys.path.insert(0, _PARENT)

from hermes_cli import prompt_stash as psmod  # noqa: E402


def build():
    return psmod.PromptStash(max_items=psmod.MAX_STASH_ITEMS, clock=lambda: build._t)

def jb(v):
    return "true" if v else "false"


def main():
    raw = sys.stdin.read()
    cases = [json.loads(line) for line in raw.splitlines() if line.strip()]
    mismatches = 0

    def run(case):
        build._t = 0.0
        if case == "build_preview":
            p1 = psmod.build_preview("hello world")
            p2 = psmod.build_preview("line1\nline2\tend")
            p3 = psmod.build_preview("a very long draft that should be ellipsized past the width limit here", 20)
            return {
                "p1": p1,
                "p2": p2,
                "p3": p3,
            }
        if case == "stash_basic":
            st = build(); build._t = 1.0; s1 = st.stash("first")
            build._t = 2.0; s2 = st.stash("second")
            return {"s1": s1, "s2": s2, "len": len(st),
                    "top_is_second": st.peek(0).text == "second"}
        if case == "pop":
            st = build(); build._t = 1.0; st.stash("first")
            build._t = 2.0; st.stash("second")
            t, im = st.pop(0)
            return {"popped": t == "second", "text": t == "second", "len_after": len(st)}
        if case == "stash_blank":
            st = build(); s = st.stash("   ")
            return {"noop": not s, "len": len(st)}
        if case == "cap":
            st = psmod.PromptStash(max_items=3, clock=lambda: build._t)
            for i in range(6):
                build._t = float(i + 1); st.stash("d%d" % i)
            return {"len": len(st), "top": st.peek(0).text == "d5"}
        if case == "panel":
            st = build()
            for i in range(3):
                build._t = float(i + 1); st.stash("d%d" % i)
            opened = st.open_panel()
            c1 = st.move_cursor(1)
            c2 = st.move_cursor(5)
            c3 = st.move_cursor(-9)
            before = len(st)
            d = st.delete_at_cursor()
            return {"opened": opened, "c1": c1, "c2_clamped": c2,
                    "c3_clamped": c3, "del": d, "before": before, "after": len(st)}
        if case == "indicator":
            st = build()
            ind0 = st.indicator(); hint0 = st.placeholder_hint()
            st.stash("draft A")
            ind1 = st.indicator(); hint1 = st.placeholder_hint()
            st.stash("draft B")
            hint2 = st.placeholder_hint()
            return {"ind0": ind0 == "", "hint0": hint0 == "",
                    "ind1": ind1 == "📌 1",
                    "hint1_has_restore": "restore" in hint1,
                    "hint2_has_browse": "browse" in hint2}
        if case == "gesture":
            st = build(); build._t = 0.0
            a1 = psmod.resolve_ctrl_s(st, "   ", None)
            a2 = psmod.resolve_ctrl_s(st, "hello", None)
            res = psmod.resolve_ctrl_s(st, "", None)
            a3, a3payload = res
            restored_text = bool(a3payload) and a3payload[0] == "hello"
            st.stash("a"); st.stash("b")
            a4 = psmod.resolve_ctrl_s(st, "", None)[0]
            panel_open = st.panel_open
            a5 = psmod.resolve_ctrl_s(st, "x", None)[0]
            return {"a1": a1[0], "a2": a2[0], "a3": a3, "a3_text": restored_text,
                    "a4": a4, "panel_open": panel_open, "a5": a5}
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
                eq = (cv is True or cv == "true" or cv == 1 or cv == "1") == ev
            elif isinstance(ev, int):
                try:
                    eq = (int(cv) == ev)
                except (TypeError, ValueError):
                    eq = False
            else:
                eq = (cv == ev)
            if not eq:
                mismatches += 1
                print("MISMATCH case=%s %s PY=%r C=%r" % (case, k, ev, cv))

    print("hermes_cli/prompt_stash oracle: %d cases, %d mismatches" % (len(cases), mismatches))
    sys.exit(1 if mismatches else 0)


if __name__ == "__main__":
    main()
