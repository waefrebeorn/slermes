"""twin for hermes_cli/dashboard_auth/public_paths.py:PUBLIC_API_PATHS (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/public_paths.py"
    PY_FN = "PUBLIC_API_PATHS"
    C_FN = "dapaths_is_public"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
