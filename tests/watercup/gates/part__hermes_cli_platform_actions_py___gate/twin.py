"""twin for hermes_cli/platform_actions.py:_gate (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/platform_actions.py"
    PY_FN = "_gate"
    C_FN = "_gate"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
