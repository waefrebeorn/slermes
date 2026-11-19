"""twin for agent/agent_runtime_helpers.py:prompt_caching_disabled_from_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "prompt_caching_disabled_from_config"
    C_FN = "agent_runtime_helpers_prompt_caching_disabled_from_config"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
