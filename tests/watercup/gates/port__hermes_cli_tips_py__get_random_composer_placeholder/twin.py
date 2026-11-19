"""twin for hermes_cli/tips.py:get_random_composer_placeholder (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/tips.py"
    PY_FN = "get_random_composer_placeholder"
    C_FN = "hermes_cli_tips_get_random_composer_placeholder"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
