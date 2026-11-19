"""twin for hermes_cli/local_runtime/hardware.py:_engine_device_pool (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/hardware.py"
    PY_FN = "_engine_device_pool"
    C_FN = "_engine_device_pool"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
