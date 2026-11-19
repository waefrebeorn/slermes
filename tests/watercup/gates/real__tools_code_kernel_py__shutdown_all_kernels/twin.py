"""twin for tools/code_kernel.py:shutdown_all_kernels (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/code_kernel.py"
    PY_FN = "shutdown_all_kernels"
    C_FN = "shutdown_all_kernels"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
