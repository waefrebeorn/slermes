"""twin for gateway/platforms/qqbot/adapter.py:send_voice (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/qqbot/adapter.py"
    PY_FN = "send_voice"
    C_FN = "send_voice"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
