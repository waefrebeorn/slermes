"""twin for agent/context_references.py:_strip_trailing_punctuation (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_references.py"
    PY_FN = "_strip_trailing_punctuation"
    C_FN = "_strip_trailing_punctuation"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
