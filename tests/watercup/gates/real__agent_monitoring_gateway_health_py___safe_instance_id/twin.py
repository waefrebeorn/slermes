"""twin: _safe_instance_id."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/gateway_health.py"; PY_FN = "_safe_instance_id"
    C_FN = "agent_monitoring_gateway_health_u_safe_instance_id"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp) if inp else None)
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
