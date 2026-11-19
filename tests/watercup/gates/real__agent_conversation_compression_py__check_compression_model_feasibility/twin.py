"""twin for agent/conversation_compression.py:check_compression_model_feasibility (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "check_compression_model_feasibility"
    C_FN = "check_compression_model_feasibility"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
