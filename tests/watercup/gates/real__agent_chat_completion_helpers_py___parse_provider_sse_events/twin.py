"""twin for agent/chat_completion_helpers.py:_parse_provider_sse_events (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/chat_completion_helpers.py"
    PY_FN = "_parse_provider_sse_events"
    C_FN = "_parse_provider_sse_events"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
