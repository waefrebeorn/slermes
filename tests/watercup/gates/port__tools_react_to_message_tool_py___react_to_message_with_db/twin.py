"""twin for tools/react_to_message_tool.py:_react_to_message_with_db (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/react_to_message_tool.py"
    PY_FN = "_react_to_message_with_db"
    C_FN = "_react_to_message_with_db"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
