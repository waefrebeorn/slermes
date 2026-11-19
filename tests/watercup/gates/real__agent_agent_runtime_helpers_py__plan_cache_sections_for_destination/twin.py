"""twin for agent/agent_runtime_helpers.py:plan_cache_sections_for_destination (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "plan_cache_sections_for_destination"
    C_FN = "plan_cache_sections_for_destination"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
