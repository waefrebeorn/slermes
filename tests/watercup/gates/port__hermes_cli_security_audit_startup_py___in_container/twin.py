"""twin for hermes_cli/security_audit_startup.py:_in_container (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/security_audit_startup.py"
    PY_FN = "_in_container"
    C_FN = "_running_in_container"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
