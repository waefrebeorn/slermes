"""twin for hermes_cli/relaunch.py:relaunch (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/relaunch.py"
    PY_FN = "relaunch"
    C_FN = "relaunch"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
