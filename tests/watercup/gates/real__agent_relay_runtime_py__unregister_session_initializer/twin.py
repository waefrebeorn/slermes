"""twin for agent/relay_runtime.py:unregister_session_initializer (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/relay_runtime.py"
    PY_FN = "unregister_session_initializer"
    C_FN = "relay_coordinator_unregister_session_initializer"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
