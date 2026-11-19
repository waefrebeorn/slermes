"""twin for tools/code_kernel.py:_drain_stderr (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/code_kernel.py"
    PY_FN = "_drain_stderr"
    C_FN = "_drain_stderr"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
