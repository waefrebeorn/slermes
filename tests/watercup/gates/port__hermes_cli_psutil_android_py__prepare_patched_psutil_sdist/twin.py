"""twin for hermes_cli/psutil_android.py:prepare_patched_psutil_sdist (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/psutil_android.py"
    PY_FN = "prepare_patched_psutil_sdist"
    C_FN = "prepare_patched_psutil_sdist"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
