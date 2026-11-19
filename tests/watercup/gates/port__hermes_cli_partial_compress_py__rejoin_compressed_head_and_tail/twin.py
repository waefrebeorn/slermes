"""twin for hermes_cli/partial_compress.py:rejoin_compressed_head_and_tail (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/partial_compress.py"
    PY_FN = "rejoin_compressed_head_and_tail"
    C_FN = "rejoin_compressed_head_and_tail"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
