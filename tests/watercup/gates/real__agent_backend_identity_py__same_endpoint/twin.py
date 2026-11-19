"""twin for agent/backend_identity.py:same_endpoint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/backend_identity.py"
    PY_FN = "same_endpoint"
    C_FN = "agent_backend_identity_same_endpoint"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
