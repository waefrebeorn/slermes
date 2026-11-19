"""twin for hermes_cli/dashboard_auth/ws_tickets.py:consume_ticket (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/ws_tickets.py"
    PY_FN = "consume_ticket"
    C_FN = "consume_ticket"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
