"""twin for agent/relay_runtime.py:_is_relay_wrapped_callback_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/relay_runtime.py"
    PY_FN = "_is_relay_wrapped_callback_error"
    C_FN = "relay_is_relay_wrapped_callback_error"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
