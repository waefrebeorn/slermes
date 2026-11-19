"""twin for hermes_cli/auth.py:invalidate_nous_auth_status_cache (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth.py"
    PY_FN = "invalidate_nous_auth_status_cache"
    C_FN = "auth_invalidate_nous_auth_status_cache"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
