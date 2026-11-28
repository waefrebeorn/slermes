"""twin: _systemd_unit_is_start_limited."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gateway.py"; PY_FN = "_systemd_unit_is_start_limited"
    C_FN = "hermes_cli_gateway_u_systemd_unit_is_start_limited"
    def call(self, inp, inputs, idx=0):
        return self._fn(_j.loads(inp))
    def norm(self, c, result):
        c.valstr("1" if result else "0")
_t = _Twin(); trace = _t.trace; run = _t.run
