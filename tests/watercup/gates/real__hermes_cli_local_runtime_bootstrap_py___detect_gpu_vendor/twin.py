"""twin for hermes_cli/local_runtime/bootstrap.py:_detect_gpu_vendor (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/bootstrap.py"
    PY_FN = "_detect_gpu_vendor"
    C_FN = "_detect_gpu_vendor"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
