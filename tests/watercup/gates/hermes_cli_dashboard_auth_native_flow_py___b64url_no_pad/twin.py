"""twin for hermes_cli/dashboard_auth/native_flow.py:_b64url_no_pad (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/native_flow.py"
    PY_FN = "_b64url_no_pad"
    C_FN = "hermes_cli_dashboard_auth_nati_u_b64url_no_pad"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
