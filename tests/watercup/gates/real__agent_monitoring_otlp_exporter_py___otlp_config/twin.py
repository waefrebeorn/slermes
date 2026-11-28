"""twin: _otlp_config over the raw input."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/monitoring/otlp_exporter.py"; PY_FN = "_otlp_config"
    C_FN = "agent_monitoring_otlp_exporter_u_otlp_config"
    def call(self, inp, inputs, idx=0):
        return self._fn(_j.loads(inp))
    def norm(self, c, result):
        c.valstr(_j.dumps(result, separators=(",",":")))
_t = _Twin(); trace = _t.trace; run = _t.run
