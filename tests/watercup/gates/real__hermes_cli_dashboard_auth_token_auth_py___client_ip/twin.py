"""twin for hermes_cli/dashboard_auth/token_auth.py:_client_ip (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/token_auth.py"
    PY_FN = "_client_ip"
    C_FN = "_client_ip"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
