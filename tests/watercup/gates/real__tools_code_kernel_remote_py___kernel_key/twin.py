"""twin for tools/code_kernel_remote.py:_kernel_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/code_kernel_remote.py"
    PY_FN = "_kernel_key"
    C_FN = "_kernel_key"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
