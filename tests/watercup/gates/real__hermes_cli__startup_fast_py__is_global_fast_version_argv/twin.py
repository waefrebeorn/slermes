"""twin: is_global_fast_version_argv."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_startup_fast.py"; PY_FN = "is_global_fast_version_argv"
    C_FN = "hermes_cli__startup_fast_u_is_global_fast_version_argv"
    def call(self, inp, inputs, idx=0):
        return self._fn(_j.loads(inp))
    def norm(self, c, result):
        c.valstr("1" if result else "0")
_t = _Twin(); trace = _t.trace; run = _t.run
