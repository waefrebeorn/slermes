"""twin: _strip_stale_thinking_for_estimate(messages) -> "role[sortedkeys]" joined by |."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/model_metadata.py"; PY_FN = "_strip_stale_thinking_for_estimate"
    C_FN = "agent_model_metadata_u_strip_stale_thinking_for_estimate"
    def call(self, inp, inputs, idx=0):
        return self._fn(_j.loads(inp))
    def norm(self, c, result):
        parts = []
        for m in result:
            role = m.get("role", "None")
            role = "None" if role is None else str(role)
            parts.append("%s[%s]" % (role, ",".join(sorted(str(k) for k in m.keys()))))
        c.valstr("|".join(parts))
_t = _Twin(); trace = _t.trace; run = _t.run
