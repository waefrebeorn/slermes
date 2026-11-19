"""twin for agent/transcript_repair.py:resolve_and_repair_transcript_batch (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transcript_repair.py"
    PY_FN = "resolve_and_repair_transcript_batch"
    C_FN = "resolve_and_repair_transcript_batch"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
