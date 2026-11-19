"""twin for hermes_cli/dashboard_auth/prefix.py:_warn_if_malformed_prefix (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/prefix.py"
    PY_FN = "_warn_if_malformed_prefix"
    C_FN = "hermes_cli_dashboard_auth_pref_u_warn_if_malformed_prefix"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
