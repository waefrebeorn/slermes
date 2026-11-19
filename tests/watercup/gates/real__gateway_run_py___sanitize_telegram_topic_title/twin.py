"""twin for gateway/run.py:_sanitize_telegram_topic_title (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_sanitize_telegram_topic_title"
    C_FN = "_sanitize_telegram_topic_title"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
