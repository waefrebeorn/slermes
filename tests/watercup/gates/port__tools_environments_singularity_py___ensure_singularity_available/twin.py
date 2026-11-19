"""twin for tools/environments/singularity.py:_ensure_singularity_available (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/singularity.py"
    PY_FN = "_ensure_singularity_available"
    C_FN = "cli_tools_environments_singularity__ensure_singularity_available"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
