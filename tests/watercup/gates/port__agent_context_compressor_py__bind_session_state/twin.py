"""twin for agent/context_compressor.py:bind_session_state (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "bind_session_state"
    C_FN = "bind_session_state"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
