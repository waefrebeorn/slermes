"""twin for hermes_cli/dashboard_auth/token_auth.py:clear_token_routes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/token_auth.py"
    PY_FN = "clear_token_routes"
    C_FN = "hermes_cli_dashboard_auth_toke_clear_token_routes"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
