"""twin for cron/jobs.py:note_fire_forward_failure (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/jobs.py"
    PY_FN = "note_fire_forward_failure"
    C_FN = "cronjobs_note_fire_forward_failure"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
