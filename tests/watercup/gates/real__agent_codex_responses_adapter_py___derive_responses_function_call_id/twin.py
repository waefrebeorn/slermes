"""twin: _derive_responses_function_call_id."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/codex_responses_adapter.py"; PY_FN = "_derive_responses_function_call_id"
    C_FN = "agent_codex_responses_adapter_u_derive_responses_function_call_id"
    def call(self, inp, inputs, idx=0):
        d=_j.loads(inp)
        return self._fn(d.get("call_id"), d.get("response_item_id"))
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
