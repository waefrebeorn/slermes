"""twin for gateway/scale_to_zero.py:dashboard_client_heartbeat_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/scale_to_zero.py"
    PY_FN = "dashboard_client_heartbeat_path"
    C_FN = "dashboard_client_heartbeat_path"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
