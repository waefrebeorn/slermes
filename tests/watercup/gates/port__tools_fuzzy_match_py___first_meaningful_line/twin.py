"""twin for tools/fuzzy_match.py:_first_meaningful_line (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/fuzzy_match.py"
    PY_FN = "_first_meaningful_line"
    C_FN = "_first_meaningful_line"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
