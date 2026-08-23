"""twin for fuzzy_strategy_ladder.

Traces the LIVE tools/fuzzy_match.py::fuzzy_find_and_replace strategy chain
with the SAME rung order as the C harness:
  F1 identical-strings short-circuit / F2 a strategy matched.
V: the LIVE strategy index ("0" = no match).
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup
import importlib.util as ilu

spec = ilu.spec_from_file_location(
    "wc_live_fuzzy", os.path.join(DEV, "tools", "fuzzy_match.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_live_fuzzy"] = mod
spec.loader.exec_module(mod)

_live = mod.fuzzy_find_and_replace


def run(inputs):
    cups = []
    for probe in inputs:
        content, old_s, new_s, ra_s = probe.split("\x1f")
        replace_all = ra_s == "1"
        c = Cup("fuzzy_strategy_ladder")
        if old_s == new_s:                                    # F1
            c.branch(True)
            c.valstr("identical")
            _, _, st, err = _live(content=content, old_string=old_s,
                                  new_string=new_s, replace_all=replace_all)
            assert err is not None and st is None, \
                "twin drift: identical not rejected"
        else:
            c.branch(False)
            _, _, strategy, err = _live(content=content, old_string=old_s,
                                        new_string=new_s,
                                        replace_all=replace_all)
            IDX = {"exact": 1, "line_trimmed": 2,
                   "whitespace_normalized": 3, "indentation_flexible": 4,
                   "escape_normalized": 5, "trimmed_boundary": 6,
                   "unicode_normalized": 7, "block_anchor": 8,
                   "context_aware": 9}
            n = IDX.get(strategy, 0) if isinstance(strategy, str) else 0
            if n > 0:                                         # F2
                c.branch(True)
                c.valstr(str(n))
            else:
                c.branch(False)
                c.valstr("0")
        cups.append(c.finalize())
    return cups
