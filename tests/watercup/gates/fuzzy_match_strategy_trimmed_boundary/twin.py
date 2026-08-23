"""twin for fuzzy_match_strategy_trimmed_boundary.

Imports LIVE tools/fuzzy_match.py::_strategy_trimmed_boundary. SCOPE NOTE:
the slermes C port trims EVERY line of the pattern/block while LIVE trims
only the first and last — the two diverge on multi-line patterns, so this
walk pins SINGLE-LINE patterns only (where trimming-all == trimming-first).
Inputs encode as "<pattern>|||<content>".
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_mod_cache = {}


def _live_fn():
    if "fn" not in _mod_cache:
        import importlib.util as ilu
        path = os.path.join(DEV, "tools", "fuzzy_match.py")
        spec = ilu.spec_from_file_location("wc_fuzzy_live", path)
        mod = ilu.module_from_spec(spec)
        sys.modules["wc_fuzzy_live"] = mod
        spec.loader.exec_module(mod)
        _mod_cache["fn"] = mod._strategy_trimmed_boundary
    return _mod_cache["fn"]


def _calc_positions(content_lines, start_line, end_line, content_length):
    start_pos = sum(len(l) + 1 for l in content_lines[:start_line])
    end_pos = sum(len(l) + 1 for l in content_lines[:end_line]) - 1
    return start_pos, min(content_length, end_pos)


def run(inputs):
    fn = _live_fn()
    cups = []
    for raw in inputs:
        pattern, content = raw.split("|||", 1)
        assert "\n" not in pattern, "scope: single-line patterns only"
        c = Cup("strategy_trimmed_boundary")
        pattern_lines = pattern.split("\n")
        c.branch(not pattern_lines)                 # b0 (split never yields [])
        matches = []
        if pattern_lines:
            modified_pattern = pattern_lines[0].strip()
            content_lines = content.split("\n")
            n = len(content_lines)
            for i in range(n - 1 + 1):              # np == 1
                c.loop()
                check = content_lines[i].strip()
                c.branch(False)                     # b1: multi-line last-trim
                is_match = check == modified_pattern
                c.branch(is_match)                  # b2
                if is_match:
                    matches.append(_calc_positions(content_lines, i, i + 1, len(content)))
        out = ",".join("%d:%d" % m for m in matches)
        live = fn(content, pattern)
        assert live == matches, "LIVE diverged on %r: %r vs %r" % (raw, live, matches)
        c.valstr(out)
        cups.append(c.finalize())
    return cups
