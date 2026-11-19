"""twin for agent/chat_completion_helpers.py:_provider_stream_text_may_be_sse (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/chat_completion_helpers.py"
    PY_FN = "_provider_stream_text_may_be_sse"
    C_FN = "_provider_stream_text_may_be_sse"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
