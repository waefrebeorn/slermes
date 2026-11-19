"""twin for agent/agent_runtime_helpers.py:convert_to_trajectory_format (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "convert_to_trajectory_format"
    C_FN = "convert_to_trajectory_format"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
