"""twin for tools/send_message_tool.py:_bounded_send_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/send_message_tool.py"
    PY_FN = "_bounded_send_error"
    C_FN = "_bounded_send_error"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
