"""twin: _uses_codex_cloud_transport."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/process_bootstrap.py"; PY_FN = "_uses_codex_cloud_transport"
    C_FN = "hermes_agent_process_bootstrap_uses_codex_cloud_transport"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp))
    def norm(self, c, result):
        c.valstr("1" if result else "0")
_t = _Twin(); trace = _t.trace; run = _t.run
