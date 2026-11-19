"""twin for agent/agent_runtime_helpers.py:invoke_tool (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "invoke_tool"
    C_FN = "invoke_tool"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
