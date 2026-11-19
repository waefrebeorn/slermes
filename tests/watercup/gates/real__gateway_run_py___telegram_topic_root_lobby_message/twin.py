"""twin for gateway/run.py:_telegram_topic_root_lobby_message (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_telegram_topic_root_lobby_message"
    C_FN = "_telegram_topic_root_lobby_message"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
