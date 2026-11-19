"""twin for agent/ssl_guard.py:_skip_ssl_guard_enabled (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/ssl_guard.py"
    PY_FN = "_skip_ssl_guard_enabled"
    C_FN = "ssl_guard_skip_enabled"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
