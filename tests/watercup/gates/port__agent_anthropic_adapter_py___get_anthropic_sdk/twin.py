"""twin for agent/anthropic_adapter.py:_get_anthropic_sdk (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_adapter.py"
    PY_FN = "_get_anthropic_sdk"
    C_FN = "cli_agent_anthropic_adapter__get_anthropic_sdk"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
