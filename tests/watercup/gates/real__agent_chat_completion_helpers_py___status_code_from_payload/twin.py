"""twin for agent/chat_completion_helpers.py:_status_code_from_payload (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/chat_completion_helpers.py"
    PY_FN = "_status_code_from_payload"
    C_FN = "_status_code_from_payload"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
