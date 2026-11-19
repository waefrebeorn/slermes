"""twin for agent/chat_completion_helpers.py:_record_interrupted_provider_wait (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/chat_completion_helpers.py"
    PY_FN = "_record_interrupted_provider_wait"
    C_FN = "_record_interrupted_provider_wait"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
