"""twin: estimate_tokens_rough(text). Input is HEX of the UTF-8 text so non-ASCII
fixtures survive argv."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/model_metadata.py"; PY_FN = "estimate_tokens_rough"
    C_FN = "agent_model_metadata_u_estimate_tokens_rough"
    def call(self, inp, inputs, idx=0):
        return self._fn(bytes.fromhex(str(inp)).decode("utf-8"))
    def norm(self, c, result):
        c.valstr(str(int(result)))
_t = _Twin(); trace = _t.trace; run = _t.run
