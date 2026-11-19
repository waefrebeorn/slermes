"""twin for agent/relay_runtime.py:retain_managed_execution (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/relay_runtime.py"
    PY_FN = "retain_managed_execution"
    C_FN = "relay_noop_retain_managed_execution"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
