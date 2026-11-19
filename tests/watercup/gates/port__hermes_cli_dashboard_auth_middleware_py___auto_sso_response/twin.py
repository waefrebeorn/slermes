"""twin for hermes_cli/dashboard_auth/middleware.py:_auto_sso_response (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/middleware.py"
    PY_FN = "_auto_sso_response"
    C_FN = "hermes_cli_dashboard_auth_midd_u_auto_sso_response"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
