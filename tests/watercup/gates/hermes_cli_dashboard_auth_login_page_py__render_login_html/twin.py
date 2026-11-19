"""twin for hermes_cli/dashboard_auth/login_page.py:render_login_html (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/login_page.py"
    PY_FN = "render_login_html"
    C_FN = "hermes_cli_dashboard_auth_logi_render_login_html"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
