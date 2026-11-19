"""twin for agent/agent_runtime_helpers.py:_is_litellm_route (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "_is_litellm_route"
    C_FN = "agent_runtime_helpers__is_litellm_route"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
