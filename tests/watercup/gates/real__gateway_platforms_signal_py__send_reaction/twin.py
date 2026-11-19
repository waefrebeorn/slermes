"""twin for gateway/platforms/signal.py:send_reaction (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/signal.py"
    PY_FN = "send_reaction"
    C_FN = "send_reaction"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
