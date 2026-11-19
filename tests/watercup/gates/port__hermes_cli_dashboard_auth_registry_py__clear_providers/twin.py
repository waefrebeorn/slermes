"""twin for hermes_cli/dashboard_auth/registry.py:clear_providers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/registry.py"
    PY_FN = "clear_providers"
    C_FN = "dash_auth_clear_providers"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
