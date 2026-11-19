"""twin for agent/agent_runtime_helpers.py:_raw_cache_ttl_from_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "_raw_cache_ttl_from_config"
    C_FN = "_raw_cache_ttl_from_config"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
