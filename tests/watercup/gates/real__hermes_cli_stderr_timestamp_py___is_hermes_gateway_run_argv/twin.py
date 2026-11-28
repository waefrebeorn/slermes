"""twin: _is_hermes_gateway_run_argv(command)."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/stderr_timestamp.py"; PY_FN = "_is_hermes_gateway_run_argv"
    C_FN = "stderr_ts_is_gateway_run_argv"
    def call(self, inp, inputs, idx=0): return self._fn(_j.loads(inp))
    def norm(self, c, result): c.valstr("True" if result else "False")
_t=_Twin(); trace=_t.trace; run=_t.run
