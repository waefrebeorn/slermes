"""twin for tools/video_generation_tool.py:_read_video_gen_section (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "tools/video_generation_tool.py"
    PY_FN = "_read_video_gen_section"
    C_FN = "cli_tools_video_generation_tool__read_video_gen_section"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run
