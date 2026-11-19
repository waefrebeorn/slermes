"""twin for tools/video_generation_tool.py:_build_dynamic_video_schema (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "tools/video_generation_tool.py"
    PY_FN = "_build_dynamic_video_schema"
    C_FN = "cli_tools_video_generation_tool__build_dynamic_video_schema"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run
