"""twin: _incident_id port twin."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "cron/incidents.py"; PY_FN = "_incident_id"
    C_FN = "cron_incidents_u_incident_id"
    def call(self, inp, inputs, idx=0):
        d=_j.loads(inp)
        return self._fn(d["job_id"], d["error_sig"])
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
