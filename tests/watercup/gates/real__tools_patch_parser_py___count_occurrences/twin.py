"""twin for tools/patch_parser.py:_count_occurrences (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/patch_parser.py"
    PY_FN = "_count_occurrences"
    C_FN = "_count_occurrences"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
