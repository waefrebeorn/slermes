"""twin for agent/agent_runtime_helpers.py:try_recover_primary_transport (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "try_recover_primary_transport"
    C_FN = "arh_try_recover_primary_transport"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
