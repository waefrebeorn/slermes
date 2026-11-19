"""twin for tools/fuzzy_match.py:_leading_whitespace (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/fuzzy_match.py"
    PY_FN = "_leading_whitespace"
    C_FN = "fuzzy_match_leading_whitespace"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
