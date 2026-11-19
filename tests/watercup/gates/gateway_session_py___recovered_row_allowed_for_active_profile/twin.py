"""twin for gateway/session.py:_recovered_row_allowed_for_active_profile (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/session.py"
    PY_FN = "_recovered_row_allowed_for_active_profile"
    C_FN = "_recovered_row_allowed_for_active_profile"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
