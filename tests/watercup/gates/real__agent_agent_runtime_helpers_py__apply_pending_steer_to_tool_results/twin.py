"""twin for agent/agent_runtime_helpers.py:apply_pending_steer_to_tool_results (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "apply_pending_steer_to_tool_results"
    C_FN = "apply_pending_steer_to_tool_results"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
