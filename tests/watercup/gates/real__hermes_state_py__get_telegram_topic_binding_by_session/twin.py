"""twin for hermes_state.py:get_telegram_topic_binding_by_session (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "get_telegram_topic_binding_by_session"
    C_FN = "get_telegram_topic_binding_by_session"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
