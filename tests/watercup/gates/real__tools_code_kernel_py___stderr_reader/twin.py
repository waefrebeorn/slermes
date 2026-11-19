"""twin for tools/code_kernel.py:_stderr_reader (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/code_kernel.py"
    PY_FN = "_stderr_reader"
    C_FN = "_stderr_reader"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
