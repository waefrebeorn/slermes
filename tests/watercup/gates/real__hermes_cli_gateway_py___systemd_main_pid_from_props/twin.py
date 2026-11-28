"""twin: _systemd_main_pid_from_props."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gateway.py"; PY_FN = "_systemd_main_pid_from_props"
    C_FN = "hermes_cli_gateway_u_systemd_main_pid_from_props"
    def call(self, inp, inputs, idx=0):
        return self._fn(_j.loads(inp))
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
