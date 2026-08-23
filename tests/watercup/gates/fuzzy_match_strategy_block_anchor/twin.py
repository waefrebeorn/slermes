"""twin for fuzzy_match_strategy_block_anchor — LIVE tools/fuzzy_match.py::_strategy_block_anchor."""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

_fn = None
try:
    import importlib.util as ilu
    spec = ilu.spec_from_file_location("wc_live_fuzzy", os.path.join(DEV, "tools", "fuzzy_match.py"))
    mod = ilu.module_from_spec(spec)
    sys.modules["wc_live_fuzzy"] = mod
    spec.loader.exec_module(mod)
    _fn = mod._strategy_block_anchor
except Exception:
    # source-extracted exec: pull the helpers this fn needs
    src = open(os.path.join(DEV, "tools", "fuzzy_match.py")).read()
    ns = {}
    from difflib import SequenceMatcher
    ns["SequenceMatcher"] = SequenceMatcher
    for name in ("_unicode_normalize", "_calculate_line_positions"):
        idx = src.index("def %s(" % name)
        nxt = src.find("\ndef ", idx + 1)
        exec(src[idx:nxt], ns)
    idx = src.index("def _strategy_block_anchor")
    nxt = src.find("\ndef ", idx + 1)
    exec(src[idx:nxt], ns)
    _fn = ns["_strategy_block_anchor"]

from watercup_py import Cup


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("_strategy_block_anchor")
        pattern, _, content = raw.partition("\t")
        result = _fn(content, pattern)
        plines = pattern.split("\n")
        c.branch(len(plines) < 2)
        if len(plines) >= 2:
            first_line = plines[0].strip()
            last_line = plines[-1].strip()
            norm_content_lines = content.split("\n")
            n = len(plines)
            ncand = 0
            for i in range(len(norm_content_lines) - n + 1):
                if norm_content_lines[i].strip() == first_line:
                    c.branch(True)
                    if norm_content_lines[i + n - 1].strip() == last_line:
                        c.branch(True)
                        ncand += 1
                    else:
                        c.branch(False)
                else:
                    c.branch(False)
            c.branch(ncand == 1)
            for _ in range(ncand):
                c.branch(n <= 2)         # similarity 1.0 vs seq_ratio path
                c.branch(bool(result))   # threshold pass
        res = ";".join("%d-%d" % (s, e) for s, e in result) or "[]"
        c.valstr(res)
        cups.append(c.finalize())
    return cups
