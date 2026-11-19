"""twin for tools/delegate_tool.py:_parent_finalization_lock (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/delegate_tool.py"
    PY_FN = "_parent_finalization_lock"
    C_FN = "dlg_parent_finalization_lock"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
