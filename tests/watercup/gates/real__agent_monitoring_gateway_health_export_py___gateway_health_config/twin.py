"""twin: _gateway_health_config over the raw input."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/gateway_health_export.py"; PY_FN = "_gateway_health_config"
    C_FN = "agent_monitoring_gateway_health_export_u_gateway_health_config"
    def call(self, inp, inputs, idx=0):
        return self._fn(_j.loads(inp))
    def norm(self, c, result):
        c.valstr(_j.dumps(result, separators=(",",":")))
_t = _Twin(); trace = _t.trace; run = _t.run
