"""twin for gateway/run.py:_format_exec_approval_fallback (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_format_exec_approval_fallback"
    C_FN = "_format_exec_approval_fallback"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
