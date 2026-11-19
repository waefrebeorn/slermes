"""twin for hermes_state.py:is_telegram_topic_mode_enabled (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "is_telegram_topic_mode_enabled"
    C_FN = "is_telegram_topic_mode_enabled"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
