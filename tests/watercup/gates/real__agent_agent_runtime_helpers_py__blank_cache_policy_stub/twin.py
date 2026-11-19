"""twin for agent/agent_runtime_helpers.py:blank_cache_policy_stub (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "blank_cache_policy_stub"
    C_FN = "blank_cache_policy_stub"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
