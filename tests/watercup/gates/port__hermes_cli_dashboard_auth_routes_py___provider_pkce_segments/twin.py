"""twin for hermes_cli/dashboard_auth/routes.py:_provider_pkce_segments (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/routes.py"
    PY_FN = "_provider_pkce_segments"
    C_FN = "_provider_pkce_segments"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
