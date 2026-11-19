"""twin for hermes_cli/heartbeat.py:resume (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/heartbeat.py"
    PY_FN = "resume"
    C_FN = "hb_manager_resume"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
