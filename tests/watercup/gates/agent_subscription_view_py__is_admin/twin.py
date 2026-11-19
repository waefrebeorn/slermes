"""twin for agent/subscription_view.py:is_admin (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/subscription_view.py"
    PY_FN = "is_admin"
    C_FN = "subview_is_admin"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
