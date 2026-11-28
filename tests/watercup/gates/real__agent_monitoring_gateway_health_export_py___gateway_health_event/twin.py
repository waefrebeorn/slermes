"""twin: _gateway_health_event."""
import json as _j, types
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/gateway_health_export.py"; PY_FN = "_gateway_health_event"
    C_FN = "agent_monitoring_gateway_health_export_u_gateway_health_event"
    def call(self, inp, inputs, idx=0):
        return self._fn(_j.loads(inp))
    def norm(self, c, result):
        c.valstr("1" if result else "0")
_t = _Twin(); trace = _t.trace; run = _t.run
