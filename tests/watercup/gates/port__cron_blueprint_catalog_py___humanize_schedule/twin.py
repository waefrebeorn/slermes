"""twin for cron/blueprint_catalog.py:_humanize_schedule (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/blueprint_catalog.py"
    PY_FN = "_humanize_schedule"
    C_FN = "blueprint_catalog_humanize_schedule"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
