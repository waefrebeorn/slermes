"""twin: _script_runner."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/verify/recipes.py"; PY_FN = "_script_runner"
    C_FN = "agent_verify_recipes_u_script_runner"
    def call(self, inp, inputs, idx=0):
        d=_j.loads(inp)
        return self._fn(d.get("package_manager"), d.get("entry") or "")
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
