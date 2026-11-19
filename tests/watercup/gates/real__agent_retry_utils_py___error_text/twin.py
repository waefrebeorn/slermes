"""twin for agent/retry_utils.py:_error_text (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/retry_utils.py"
    PY_FN = "_error_text"
    C_FN = "_error_text"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
