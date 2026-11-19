"""twin for gateway/message_timestamps.py:strip_leading_message_timestamps (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/message_timestamps.py"
    PY_FN = "strip_leading_message_timestamps"
    C_FN = "strip_leading_message_timestamps"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
