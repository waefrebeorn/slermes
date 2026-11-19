"""twin for hermes_cli/dashboard_auth/registry.py:unregister_global_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/registry.py"
    PY_FN = "unregister_global_provider"
    C_FN = "dash_auth_unregister_global_provider"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
