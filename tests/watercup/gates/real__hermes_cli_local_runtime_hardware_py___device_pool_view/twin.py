"""twin for hermes_cli/local_runtime/hardware.py:_device_pool_view (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/hardware.py"
    PY_FN = "_device_pool_view"
    C_FN = "_device_pool_view"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
