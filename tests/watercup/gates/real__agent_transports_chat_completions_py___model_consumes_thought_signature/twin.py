"""twin for agent/transports/chat_completions.py:_model_consumes_thought_signature (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/chat_completions.py"
    PY_FN = "_model_consumes_thought_signature"
    C_FN = "cct_model_consumes_thought_signature"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
