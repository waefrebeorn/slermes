"""twin for gateway/stream_consumer.py:_try_strip_cursor (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/stream_consumer.py"
    PY_FN = "_try_strip_cursor"
    C_FN = "sc_try_strip_cursor"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
