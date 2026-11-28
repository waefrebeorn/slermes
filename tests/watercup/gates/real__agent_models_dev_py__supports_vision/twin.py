"""twin for agent/models_dev.py capability predicates (self-only). Input = JSON."""
import json as _j, types
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/models_dev.py"
    PY_FN = "ModelInfo.supports_vision"
    C_FN = "agent_models_dev_u_supports_vision"
    def call(self, inp, inputs, idx=0):
        d = _j.loads(inp)
        obj = types.SimpleNamespace(
            cost_input=d.get("cost_input", 0.0), cost_output=d.get("cost_output", 0.0),
            attachment=d.get("attachment", False), input_modalities=d.get("input_modalities", []))
        return self._fn(obj)
    def norm(self, c, result):
        c.valstr("1" if result else "0")
_t = _Twin(); trace = _t.trace; run = _t.run
