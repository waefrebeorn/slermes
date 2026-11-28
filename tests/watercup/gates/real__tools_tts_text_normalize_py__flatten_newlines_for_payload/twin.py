"""twin: flatten_newlines_for_payload(text). Input is HEX of the UTF-8 text."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "tools/tts_text_normalize.py"; PY_FN = "flatten_newlines_for_payload"
    C_FN = "agent_tts_text_normalize_u_flatten_newlines_for_payload"
    def call(self, inp, inputs, idx=0):
        return self._fn(bytes.fromhex(str(inp)).decode("utf-8"))
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
