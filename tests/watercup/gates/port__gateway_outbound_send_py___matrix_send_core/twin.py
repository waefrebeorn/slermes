"""twin for tools/send_message_tool.py:_matrix_send_core (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/send_message_tool.py"
    PY_FN = "_matrix_send_core"
    C_FN = "_matrix_send_core"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
