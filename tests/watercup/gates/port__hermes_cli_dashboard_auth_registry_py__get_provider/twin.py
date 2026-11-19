"""twin for hermes_cli/dashboard_auth/registry.py:get_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/registry.py"
    PY_FN = "get_provider"
    C_FN = "dash_auth_get_provider"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
