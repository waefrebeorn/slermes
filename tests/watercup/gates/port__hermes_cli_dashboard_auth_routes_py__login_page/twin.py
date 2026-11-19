"""twin for hermes_cli/dashboard_auth/routes.py:login_page (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/routes.py"
    PY_FN = "login_page"
    C_FN = "hermes_cli_dashboard_auth_rout_login_page"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
