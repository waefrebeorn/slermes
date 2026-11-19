"""twin for hermes_cli/auth.py:_log_nous_invoke_jwt_selected (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth.py"
    PY_FN = "_log_nous_invoke_jwt_selected"
    C_FN = "auth_log_nous_invoke_jwt_selected"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
