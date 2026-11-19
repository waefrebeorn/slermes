"""twin for agent/chat_completion_helpers.py:interruptible_streaming_api_call (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/chat_completion_helpers.py"
    PY_FN = "interruptible_streaming_api_call"
    C_FN = "interruptible_streaming_api_call"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
