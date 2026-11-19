"""twin for gateway/run.py:_schedule_plugin_message_injection (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_schedule_plugin_message_injection"
    C_FN = "_schedule_plugin_message_injection"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
