"""twin: _normalize_base_url(text) over the raw input string."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/model_metadata.py"; PY_FN = "_normalize_base_url"
    C_FN = "agent_model_metadata_u_normalize_base_url"
    def call(self, inp, inputs, idx=0):
        return self._fn(inp)
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
