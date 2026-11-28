"""twin: _is_cjk_token_dense_char(ch) -> bool. Input is HEX of the UTF-8 text."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/model_metadata.py"; PY_FN = "_is_cjk_token_dense_char"
    C_FN = "agent_model_metadata_u_is_cjk_token_dense_char"
    def call(self, inp, inputs, idx=0):
        return self._fn(bytes.fromhex(str(inp)).decode("utf-8"))
    def norm(self, c, result):
        c.valstr("1" if result else "0")
_t = _Twin(); trace = _t.trace; run = _t.run
