"""twin for tools/vision_tools.py:_run_encode_on_cpu_executor (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/vision_tools.py"
    PY_FN = "_run_encode_on_cpu_executor"
    C_FN = "_run_encode_on_cpu_executor"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
