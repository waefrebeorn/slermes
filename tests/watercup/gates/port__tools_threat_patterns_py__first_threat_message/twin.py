"""twin for tools/threat_patterns.py:first_threat_message (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/threat_patterns.py"
    PY_FN = "first_threat_message"
    C_FN = "cli_tools_threat_patterns_first_threat_message"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
