"""twin for tools/image_generation_tool.py:_resolve_managed_fal_gateway (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "tools/image_generation_tool.py"
    PY_FN = "_resolve_managed_fal_gateway"
    C_FN = "image_gen_resolve_managed_fal_gateway"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run
