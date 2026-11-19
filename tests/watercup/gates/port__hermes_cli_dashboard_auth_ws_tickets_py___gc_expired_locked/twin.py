"""twin for hermes_cli/dashboard_auth/ws_tickets.py:_gc_expired_locked (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/ws_tickets.py"
    PY_FN = "_gc_expired_locked"
    C_FN = "ws_tickets__gc_expired_locked"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
