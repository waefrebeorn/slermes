"""twin for gateway/status.py:_build_runtime_status_record (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "gateway/status.py"
    PY_FN = "_build_runtime_status_record"
    C_FN = "build_runtime_status_record"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run
