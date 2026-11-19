"""twin for hermes_cli/update_cmd.py:_relaunch_stopped_serves (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_relaunch_stopped_serves"
    C_FN = "_relaunch_stopped_serves"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
