"""twin: _format_responses_error(error_obj, response_status)."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/codex_responses_adapter.py"; PY_FN = "_format_responses_error"
    C_FN = "agent_codex_responses_u_format_responses_error"
    def call(self, inp, inputs, idx=0):
        d = _j.loads(inp)
        return self._fn(d.get("error"), d.get("status", ""))
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
