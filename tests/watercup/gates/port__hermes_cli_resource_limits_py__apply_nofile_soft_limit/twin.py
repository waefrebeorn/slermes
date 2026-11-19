"""twin for hermes_cli/resource_limits.py:apply_nofile_soft_limit (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/resource_limits.py"
    PY_FN = "apply_nofile_soft_limit"
    C_FN = "apply_nofile_soft_limit"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
