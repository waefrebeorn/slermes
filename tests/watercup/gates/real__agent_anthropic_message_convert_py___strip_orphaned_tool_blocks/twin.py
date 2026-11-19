"""twin for agent/anthropic_message_convert.py:_strip_orphaned_tool_blocks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_message_convert.py"
    PY_FN = "_strip_orphaned_tool_blocks"
    C_FN = "_strip_orphaned_tool_blocks"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
