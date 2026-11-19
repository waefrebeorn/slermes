"""twin for agent/prompt_cache_scope.py:_agent_source (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/prompt_cache_scope.py"
    PY_FN = "_agent_source"
    C_FN = "_agent_source"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
