"""twin for agent/transports/chat_completions.py:convert_messages (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/chat_completions.py"
    PY_FN = "convert_messages"
    C_FN = "convert_messages"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
