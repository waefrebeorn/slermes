"""twin for hermes_cli/telegram_managed_bot.py:is_valid_telegram_bot_token (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/telegram_managed_bot.py"
    PY_FN = "is_valid_telegram_bot_token"
    C_FN = "is_valid_telegram_bot_token"

    def norm(self, c, result):
        import inspect as _ic
        if _ic.iscoroutine(result):
            import asyncio as _aio
            result = _aio.run(result)
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
