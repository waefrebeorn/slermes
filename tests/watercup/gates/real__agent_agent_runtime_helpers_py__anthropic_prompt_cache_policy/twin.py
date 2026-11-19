"""twin for agent/agent_runtime_helpers.py:anthropic_prompt_cache_policy (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "anthropic_prompt_cache_policy"
    C_FN = "anthropic_prompt_cache_policy"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
