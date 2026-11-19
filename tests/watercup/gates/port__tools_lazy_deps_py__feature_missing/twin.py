"""twin for tools/lazy_deps.py:feature_missing (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "tools/lazy_deps.py"
    PY_FN = "feature_missing"
    C_FN = "lazy_feature_missing"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run
