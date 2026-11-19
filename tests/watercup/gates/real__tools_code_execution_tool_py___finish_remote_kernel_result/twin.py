"""twin for tools/code_execution_tool.py:_finish_remote_kernel_result (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/code_execution_tool.py"
    PY_FN = "_finish_remote_kernel_result"
    C_FN = "_finish_remote_kernel_result"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
