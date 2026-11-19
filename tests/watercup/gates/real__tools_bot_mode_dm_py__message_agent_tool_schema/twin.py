"""twin for tools/bot_mode_dm.py:message_agent_tool_schema (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/bot_mode_dm.py"
    PY_FN = "message_agent_tool_schema"
    C_FN = "message_agent_tool_schema"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
