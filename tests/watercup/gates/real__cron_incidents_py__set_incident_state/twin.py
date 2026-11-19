"""twin for cron/incidents.py:set_incident_state (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/incidents.py"
    PY_FN = "set_incident_state"
    C_FN = "set_incident_state"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
