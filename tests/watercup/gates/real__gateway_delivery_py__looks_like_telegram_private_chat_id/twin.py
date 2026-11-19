"""twin for gateway/delivery.py:looks_like_telegram_private_chat_id (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/delivery.py"
    PY_FN = "looks_like_telegram_private_chat_id"
    C_FN = "looks_like_telegram_private_chat_id"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
