"""twin for hermes_cli/colors.py:should_use_color (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/colors.py"
    PY_FN = "should_use_color"
    C_FN = "should_use_color"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
