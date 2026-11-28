"""twin: PhaseResult.ok (property) over a JSON phase result."""
import json as _j, types
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/verify/runner.py"; PY_FN = "PhaseResult.ok"
    C_FN = "agent_verify_runner_u_ok"
    def call(self, inp, inputs, idx=0):
        d = _j.loads(inp)
        recv = types.SimpleNamespace(exit_code=d.get("exit_code"), timed_out=d.get("timed_out", False))
        return self._fn(recv)
    def norm(self, c, result):
        c.valstr("1" if result else "0")
_t = _Twin(); trace = _t.trace; run = _t.run
