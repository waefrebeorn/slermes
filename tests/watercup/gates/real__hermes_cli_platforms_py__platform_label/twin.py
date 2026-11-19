"""twin for hermes_cli/platforms.py:platform_label (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/platforms.py"
    PY_FN = "platform_label"
    C_FN = "platform_label"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
