"""twin for hermes_cli/tips.py:get_random_tip (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/tips.py"
    PY_FN = "get_random_tip"
    C_FN = "get_random_tip"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
