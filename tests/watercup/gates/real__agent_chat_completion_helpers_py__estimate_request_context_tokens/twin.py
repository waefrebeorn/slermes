"""twin for agent/chat_completion_helpers.py:estimate_request_context_tokens (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/chat_completion_helpers.py"
    PY_FN = "estimate_request_context_tokens"
    C_FN = "cch_estimate_request_context_tokens"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
