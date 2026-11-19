"""twin for agent/relay_runtime.py:_clear_relay_plugins (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/relay_runtime.py"
    PY_FN = "_clear_relay_plugins"
    C_FN = "relay_clear_relay_plugins"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
