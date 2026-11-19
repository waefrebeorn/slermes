"""twin for agent/chat_completion_helpers.py:build_assistant_message (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/chat_completion_helpers.py"
    PY_FN = "build_assistant_message"
    C_FN = "build_assistant_message"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
