"""twin for tools/send_message_tool.py:_is_telegram_thread_not_found (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/send_message_tool.py"
    PY_FN = "_is_telegram_thread_not_found"
    C_FN = "tools_is_telegram_thread_not_found"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
