"""twin for gateway/agent_cache_pressure.py:_cgroup_limit_bytes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/agent_cache_pressure.py"
    PY_FN = "_cgroup_limit_bytes"
    C_FN = "acb_cgroup_limit_bytes"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
