"""twin for fuzzy_match_find_closest_lines.

Traces the LIVE tools/fuzzy_match.py::find_closest_lines with the SAME rung
order as the C harness (transcribed control structure, live semantics):

  B1 empty old/content / B2 no lines / B3 first-line-blank + fallback scan
  L,B4 non-blank / B5 no anchor / scoring L,B6 blank-skip,B7 ratio>0.3 /
  B8 nothing scored / selection L,B9 duplicate-range / B10 nothing emitted /
  V selected 1-based content line numbers.

V carries selected indices (not the formatted string) because the C port
emits a trailing newline per block while LIVE does not — documented gap.
Input: JSON [old_string, content, context_lines, max_results].
"""
import os
import json
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec_path = os.path.join(DEV, "tools", "fuzzy_match.py")
import importlib.util as _ilu
_spec = _ilu.spec_from_file_location("wc_live_fuzzy_match", _spec_path)
_fm = _ilu.module_from_spec(_spec)
sys.modules["wc_live_fuzzy_match"] = _fm
_spec.loader.exec_module(_fm)


def trace(old_string, content, context_lines, max_results):
    c = Cup("find_closest_lines")
    selected = []

    c.branch(not old_string or not content)                    # B1
    if not old_string or not content:
        return c._done(selected)

    old_lines = old_string.splitlines()
    content_lines = content.splitlines()

    c.branch(not old_lines or not content_lines)               # B2
    if not old_lines or not content_lines:
        return c._done(selected)

    # anchor: first line stripped; if blank, first non-blank stripped
    anchor = old_lines[0].strip()
    c.branch(not anchor)                                       # B3
    if not anchor:
        candidates = []
        for line in old_lines:
            c.loop()
            stripped = line.strip()
            nonblank = bool(stripped)
            c.branch(nonblank)                                 # B4
            if nonblank:
                candidates.append(stripped)
        c.branch(not candidates)                               # B5
        if not candidates:
            return c._done(selected)
        anchor = candidates[0]

    scored = []
    for i, line in enumerate(content_lines):
        c.loop()
        stripped = line.strip()
        blank = not stripped
        c.branch(blank)                                        # B6
        ratio = 0.0
        if not blank:
            from difflib import SequenceMatcher
            ratio = SequenceMatcher(None, anchor, stripped).ratio()
            hits = ratio > 0.3
            c.branch(hits)                                     # B7
            if hits:
                scored.append((ratio, i))

    c.branch(not scored)                                       # B8
    if not scored:
        return c._done(selected)

    scored.sort(key=lambda x: -x[0])
    top = scored[:max_results]

    seen_ranges = set()
    emitted = 0
    for _, line_idx in top:
        c.loop()
        start = max(0, line_idx - context_lines)
        end = min(len(content_lines), line_idx + len(old_lines) + context_lines)
        key = (start, end)
        dup = key in seen_ranges
        c.branch(dup)                                          # B9
        if dup:
            continue
        seen_ranges.add(key)
        selected.append(line_idx + 1)
        emitted += 1

    c.branch(emitted == 0)                                     # B10
    return c._done(selected)


def _done(self, selected):
    self.valstr(".".join(str(i) for i in selected))
    return self.finalize()


Cup._done = _done


def run(inputs):
    out = []
    for raw in inputs:
        arr = json.loads(raw)
        old_s, cont = arr[0], arr[1]
        ctx = int(arr[2]) if len(arr) > 2 else 2
        maxr = int(arr[3]) if len(arr) > 3 else 3
        out.append(trace(old_s, cont, ctx, maxr))
    return out


if __name__ == "__main__":
    cases = [
        ("def foo():", "def foo():\n    x = 1\n    foo()", 2, 3),
        ("", "abc", 2, 3),
        ("zzzz", "alpha\nbeta", 1, 3),
    ]
    for old_s, cont, ctx, maxr in cases:
        assert _fm.find_closest_lines(old_s, cont, ctx, maxr) is not None
    print("twin ok")
