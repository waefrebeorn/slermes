"""twin for cron/scheduler.py:_get_bot_chat_delivery_timeout (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_get_bot_chat_delivery_timeout"
    C_FN = "_get_bot_chat_delivery_timeout"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
