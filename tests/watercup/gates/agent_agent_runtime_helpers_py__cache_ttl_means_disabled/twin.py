"""twin for agent/agent_runtime_helpers.py:cache_ttl_means_disabled (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "cache_ttl_means_disabled"
    C_FN = "agent_agent_runtime_helpers_cache_ttl_means_disabled"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
