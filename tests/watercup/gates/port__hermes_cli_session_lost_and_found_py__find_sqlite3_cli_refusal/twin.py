"""twin for hermes_cli/session_lost_and_found.py:find_sqlite3_cli_refusal (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_lost_and_found.py"
    PY_FN = "find_sqlite3_cli_refusal"
    C_FN = "find_sqlite3_cli_refusal"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
