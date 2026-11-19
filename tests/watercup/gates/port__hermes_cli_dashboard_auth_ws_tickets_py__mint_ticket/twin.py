"""twin for hermes_cli/dashboard_auth/ws_tickets.py:mint_ticket (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/ws_tickets.py"
    PY_FN = "mint_ticket"
    C_FN = "mint_ticket"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
