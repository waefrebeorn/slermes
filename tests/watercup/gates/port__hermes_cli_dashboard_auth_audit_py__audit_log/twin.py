"""twin for hermes_cli/dashboard_auth/audit.py:audit_log (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/audit.py"
    PY_FN = "audit_log"
    C_FN = "fields"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
