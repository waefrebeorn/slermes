"""twin: _prepare_child_command(command, environ)."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/stderr_timestamp.py"; PY_FN = "_prepare_child_command"
    C_FN = "stderr_ts_prepare_child_command"
    def call(self, inp, inputs, idx=0):
        d=_j.loads(inp); env={"XPC_SERVICE_NAME": d.get("xpc","")}
        return self._fn(d.get("argv") or [], env)
    def norm(self, c, result): c.valstr(_j.dumps(list(result), separators=(",",":")))
_t=_Twin(); trace=_t.trace; run=_t.run
