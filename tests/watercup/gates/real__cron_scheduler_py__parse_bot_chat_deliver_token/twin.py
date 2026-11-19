"""twin for cron/scheduler.py:parse_bot_chat_deliver_token (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "parse_bot_chat_deliver_token"
    C_FN = "parse_bot_chat_deliver_token"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
