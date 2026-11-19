"""twin for hermes_cli/nous_subscription.py:_selected_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/nous_subscription.py"
    PY_FN = "_selected_provider"
    C_FN = "_selected_provider"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
