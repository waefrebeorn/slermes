"""twin for agent/agent_runtime_helpers.py:agent_runtime_owns_post_tool_hook (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "agent_runtime_owns_post_tool_hook"
    C_FN = "arh_agent_runtime_owns_post_tool_hook"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
