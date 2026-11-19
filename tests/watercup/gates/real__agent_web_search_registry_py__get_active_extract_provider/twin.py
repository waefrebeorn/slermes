"""twin for agent/web_search_registry.py:get_active_extract_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/web_search_registry.py"
    PY_FN = "get_active_extract_provider"
    C_FN = "web_search_get_active_extract_provider"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
