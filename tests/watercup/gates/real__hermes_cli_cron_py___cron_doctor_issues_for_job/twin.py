"""twin for hermes_cli/cron.py:_cron_doctor_issues_for_job (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/cron.py"
    PY_FN = "_cron_doctor_issues_for_job"
    C_FN = "_cron_doctor_issues_for_job"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
