"""twin for agent/relay_runtime.py:finalize_conversation (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/relay_runtime.py"
    PY_FN = "finalize_conversation"
    C_FN = "relay_coordinator_finalize_conversation"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
