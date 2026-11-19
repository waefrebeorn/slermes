"""twin for hermes_cli/dashboard_auth/registry.py:restore_registration (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/registry.py"
    PY_FN = "restore_registration"
    C_FN = "dash_auth_restore_registration"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
