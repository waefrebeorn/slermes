"""twin for hermes_state.py:list_telegram_topic_bindings_for_chat (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "list_telegram_topic_bindings_for_chat"
    C_FN = "list_telegram_topic_bindings_for_chat"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
