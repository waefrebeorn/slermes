"""twin for agent/agent_runtime_helpers.py:_classify_tool_call_orphans (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "_classify_tool_call_orphans"
    C_FN = "_classify_tool_call_orphans"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
