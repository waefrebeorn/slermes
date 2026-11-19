"""twin for gateway/run.py:_load_signal_interrupt_grace_timeout (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_load_signal_interrupt_grace_timeout"
    C_FN = "_load_signal_interrupt_grace_timeout"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
