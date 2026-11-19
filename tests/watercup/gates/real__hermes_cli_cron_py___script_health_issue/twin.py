"""twin for hermes_cli/cron.py:_script_health_issue (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/cron.py"
    PY_FN = "_script_health_issue"
    C_FN = "_script_health_issue"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
