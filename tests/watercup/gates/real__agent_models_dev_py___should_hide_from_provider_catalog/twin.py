"""twin: _should_hide_from_provider_catalog(provider, model_id) -> bool."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/models_dev.py"; PY_FN = "_should_hide_from_provider_catalog"
    C_FN = "agent_models_dev_u_should_hide_from_provider_catalog"
    def call(self, inp, inputs, idx=0):
        d = _j.loads(inp)
        return self._fn(d.get("provider"), d.get("model_id"))
    def norm(self, c, result):
        c.valstr("1" if result else "0")
_t = _Twin(); trace = _t.trace; run = _t.run
