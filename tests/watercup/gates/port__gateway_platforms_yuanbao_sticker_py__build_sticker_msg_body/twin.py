"""twin for gateway/platforms/yuanbao_sticker.py:build_sticker_msg_body (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/yuanbao_sticker.py"
    PY_FN = "build_sticker_msg_body"
    C_FN = "cli_gateway_platforms_yuanbao_sticker_build_sticker_msg_body"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run
