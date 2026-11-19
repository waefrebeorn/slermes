"""twin for agent/pet/render.py:_open_sheet (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/render.py"
    PY_FN = "_open_sheet"
    C_FN = "pet_open_sheet"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
