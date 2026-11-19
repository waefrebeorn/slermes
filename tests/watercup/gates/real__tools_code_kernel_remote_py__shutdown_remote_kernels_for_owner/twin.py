"""twin for tools/code_kernel_remote.py:shutdown_remote_kernels_for_owner (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/code_kernel_remote.py"
    PY_FN = "shutdown_remote_kernels_for_owner"
    C_FN = "shutdown_remote_kernels_for_owner"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
