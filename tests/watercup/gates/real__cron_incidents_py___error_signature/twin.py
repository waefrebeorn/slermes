"""twin: _error_signature."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "cron/incidents.py"; PY_FN = "_error_signature"
    C_FN = "cron_incidents_u_error_signature"
    def call(self, inp, inputs, idx=0):
        d=_j.loads(inp)
        return self._fn(d.get("job_id",""), d.get("error",""))
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
