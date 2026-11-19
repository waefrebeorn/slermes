"""twin for gateway/turn_lease.py:_get_or_create (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/turn_lease.py"
    PY_FN = "_get_or_create"
    C_FN = "request_restart"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
