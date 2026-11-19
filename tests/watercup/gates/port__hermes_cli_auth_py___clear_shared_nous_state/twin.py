"""twin for hermes_cli/auth.py:_clear_shared_nous_state (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth.py"
    PY_FN = "_clear_shared_nous_state"
    C_FN = "auth_clear_shared_nous_state"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
