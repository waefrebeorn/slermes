"""twin for hermes_cli/dashboard_auth/cookies.py:_clear_cookie_variants (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_auth/cookies.py"
    PY_FN = "_clear_cookie_variants"
    C_FN = "_clear_cookie_variants"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
