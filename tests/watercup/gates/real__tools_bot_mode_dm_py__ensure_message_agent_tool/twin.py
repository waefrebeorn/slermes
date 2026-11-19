"""twin for tools/bot_mode_dm.py:ensure_message_agent_tool (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/bot_mode_dm.py"
    PY_FN = "ensure_message_agent_tool"
    C_FN = "ensure_message_agent_tool"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
