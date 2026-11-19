"""twin for cron/incidents.py:ack_incident (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/incidents.py"
    PY_FN = "ack_incident"
    C_FN = "ack_incident"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
