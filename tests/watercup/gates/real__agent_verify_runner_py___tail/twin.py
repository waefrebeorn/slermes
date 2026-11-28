"""twin: _tail port twin."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/verify/runner.py"; PY_FN = "_tail"
    C_FN = "agent_verify_runner_u_tail"
    def call(self, inp, inputs, idx=0):
        d=_j.loads(inp)
        return self._fn(d["text"], d["limit"])
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
