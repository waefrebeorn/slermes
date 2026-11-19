"""twin for agent/relay_runtime.py:_resolve_plugin_awaitable (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/relay_runtime.py"
    PY_FN = "_resolve_plugin_awaitable"
    C_FN = "relay_resolve_plugin_awaitable"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
