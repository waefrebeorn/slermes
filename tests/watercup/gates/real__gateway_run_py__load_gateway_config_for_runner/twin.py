"""twin for gateway/run.py:load_gateway_config_for_runner (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "load_gateway_config_for_runner"
    C_FN = "gw_load_gateway_config_for_runner"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
