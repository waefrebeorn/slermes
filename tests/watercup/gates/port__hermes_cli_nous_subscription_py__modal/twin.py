"""twin for hermes_cli/nous_subscription.py:modal (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/nous_subscription.py"
    PY_FN = "modal"
    C_FN = "nsub_modal"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
