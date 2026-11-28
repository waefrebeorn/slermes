"""twin: platform_for_subsystem(text) over the raw input string."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/gateway_health.py"; PY_FN = "platform_for_subsystem"
    C_FN = "agent_monitoring_gateway_health_u_platform_for_subsystem"
    def call(self, inp, inputs, idx=0):
        return self._fn(inp)
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
