"""twin for hermes_cli/dashboard_auth/native_flow.py:_s256 (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/native_flow.py"
    PY_FN = "_s256"
    C_FN = "hermes_cli_dashboard_auth_nati_u_s256"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
