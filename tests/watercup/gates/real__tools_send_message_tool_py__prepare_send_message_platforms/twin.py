"""twin for tools/send_message_tool.py:prepare_send_message_platforms (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/send_message_tool.py"
    PY_FN = "prepare_send_message_platforms"
    C_FN = "prepare_send_message_platforms"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
