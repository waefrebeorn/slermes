"""twin for tools/react_to_message_tool.py:react_to_message_tool (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/react_to_message_tool.py"
    PY_FN = "react_to_message_tool"
    C_FN = "react_to_message_tool"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
