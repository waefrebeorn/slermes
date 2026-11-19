"""twin for tools/react_to_message_tool.py:_open_session_db (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/react_to_message_tool.py"
    PY_FN = "_open_session_db"
    C_FN = "_open_session_db"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
