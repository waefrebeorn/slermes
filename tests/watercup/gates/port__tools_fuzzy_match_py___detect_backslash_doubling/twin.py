"""twin for tools/fuzzy_match.py:_detect_backslash_doubling (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/fuzzy_match.py"
    PY_FN = "_detect_backslash_doubling"
    C_FN = "_detect_backslash_doubling"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
