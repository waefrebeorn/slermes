"""twin for hermes_cli/auth.py:_warn_malformed_secret (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth.py"
    PY_FN = "_warn_malformed_secret"
    C_FN = "ath_warn_malformed_secret"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
