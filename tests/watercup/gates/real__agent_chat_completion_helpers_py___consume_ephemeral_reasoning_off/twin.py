"""twin for agent/chat_completion_helpers.py:_consume_ephemeral_reasoning_off (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/chat_completion_helpers.py"
    PY_FN = "_consume_ephemeral_reasoning_off"
    C_FN = "_consume_ephemeral_reasoning_off"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
