"""twin for hermes_cli/heartbeat.py:clear (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/heartbeat.py"
    PY_FN = "clear"
    C_FN = "hb_manager_clear"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
