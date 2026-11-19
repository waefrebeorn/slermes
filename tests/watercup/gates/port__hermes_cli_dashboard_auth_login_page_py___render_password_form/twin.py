"""twin for hermes_cli/dashboard_auth/login_page.py:_render_password_form (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/login_page.py"
    PY_FN = "_render_password_form"
    C_FN = "hermes_cli_dashboard_auth_logi_u_render_password_form"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
