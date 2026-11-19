"""twin for agent/relay_runtime.py:_flush_relay_subscribers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/relay_runtime.py"
    PY_FN = "_flush_relay_subscribers"
    C_FN = "relay_flush_relay_subscribers"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
