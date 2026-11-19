"""twin for hermes_cli/callbacks.py:approval_callback (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/callbacks.py"
    PY_FN = "approval_callback"
    C_FN = "approval_callback"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
