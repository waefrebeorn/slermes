"""twin for tools/send_message_tool.py:resolve_send_target (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/send_message_tool.py"
    PY_FN = "resolve_send_target"
    C_FN = "resolve_send_target"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
