"""twin: _normalize_service_definition."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gateway.py"; PY_FN = "_normalize_service_definition"
    C_FN = "hermes_cli_gateway_u_normalize_service_definition"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp))
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
