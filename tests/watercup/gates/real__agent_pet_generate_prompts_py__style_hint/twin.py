"""twin: style_hint over the raw input."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/pet/generate/prompts.py"; PY_FN = "style_hint"
    C_FN = "agent_pet_generate_prompts_u_style_hint"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp))
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
