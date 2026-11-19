"""twin for agent/transports/chat_completions.py:validate_response (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/chat_completions.py"
    PY_FN = "validate_response"
    C_FN = "cct_validate_response"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
