"""twin for hermes_cli/security_audit.py:run_audit (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/security_audit.py"
    PY_FN = "run_audit"
    C_FN = "run_audit"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
