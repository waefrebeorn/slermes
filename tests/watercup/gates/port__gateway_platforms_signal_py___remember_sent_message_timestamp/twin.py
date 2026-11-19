"""twin for gateway/platforms/signal.py:_remember_sent_message_timestamp (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/signal.py"
    PY_FN = "_remember_sent_message_timestamp"
    C_FN = "_remember_sent_message_timestamp"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
