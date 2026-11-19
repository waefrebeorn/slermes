"""twin for gateway/wake.py:adapter_supports_push (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/wake.py"
    PY_FN = "adapter_supports_push"
    C_FN = "gateway_wake_adapter_supports_push"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
