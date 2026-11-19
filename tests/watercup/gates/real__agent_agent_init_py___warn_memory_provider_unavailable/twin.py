"""twin for agent/agent_init.py:_warn_memory_provider_unavailable (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_init.py"
    PY_FN = "_warn_memory_provider_unavailable"
    C_FN = "_warn_memory_provider_unavailable"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
