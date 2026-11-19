"""twin for gateway/session_context.py:async_delivery_supported (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/session_context.py"
    PY_FN = "async_delivery_supported"
    C_FN = "gw_session_async_delivery_supported"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
