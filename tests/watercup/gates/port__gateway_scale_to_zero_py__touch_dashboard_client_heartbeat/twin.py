"""twin for gateway/scale_to_zero.py:touch_dashboard_client_heartbeat (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/scale_to_zero.py"
    PY_FN = "touch_dashboard_client_heartbeat"
    C_FN = "touch_dashboard_client_heartbeat"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
