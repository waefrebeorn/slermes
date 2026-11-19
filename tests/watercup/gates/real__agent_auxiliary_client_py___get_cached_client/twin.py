"""twin for agent/auxiliary_client.py:_get_cached_client (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_get_cached_client"
    C_FN = "aux__get_cached_client"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
