"""twin for agent/moa_loop.py:_extract_text (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/moa_loop.py"
    PY_FN = "_extract_text"
    C_FN = "_extract_text"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
