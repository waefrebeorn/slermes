"""twin for hermes_cli/dashboard_auth/token_auth.py:authenticate_token (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/token_auth.py"
    PY_FN = "authenticate_token"
    C_FN = "hermes_cli_dashboard_auth_toke_authenticate_token"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
