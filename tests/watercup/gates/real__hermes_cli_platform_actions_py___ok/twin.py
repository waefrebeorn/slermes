"""twin for hermes_cli/platform_actions.py:_ok (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/platform_actions.py"
    PY_FN = "_ok"
    C_FN = "_ok"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
