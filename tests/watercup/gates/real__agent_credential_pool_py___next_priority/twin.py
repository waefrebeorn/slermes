"""twin: _next_priority."""
import json as _j, types
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/credential_pool.py"; PY_FN = "_next_priority"
    C_FN = "agent_credential_pool_u_next_priority"
    def call(self, inp, inputs, idx=0):
        raw=_j.loads(inp)
        es=[types.SimpleNamespace(priority=(r['priority'] if isinstance(r,dict) else r)) for r in raw]
        return self._fn(es)
    def norm(self, c, result):
        c.valstr(str(int(result)))
_t = _Twin(); trace = _t.trace; run = _t.run
