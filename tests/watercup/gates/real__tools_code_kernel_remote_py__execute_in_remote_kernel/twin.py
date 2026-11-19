"""twin for tools/code_kernel_remote.py:execute_in_remote_kernel (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/code_kernel_remote.py"
    PY_FN = "execute_in_remote_kernel"
    C_FN = "execute_in_remote_kernel"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
