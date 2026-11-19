"""twin for hermes_cli/dashboard_auth/audit.py:_resolve_log_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/audit.py"
    PY_FN = "_resolve_log_path"
    C_FN = "_resolve_log_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
