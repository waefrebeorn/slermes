"""twin for hermes_cli/dashboard_auth/base.py:revoke_session (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/base.py"
    PY_FN = "revoke_session"
    C_FN = "hermes_cli_dashboard_auth_base_revoke_session"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
