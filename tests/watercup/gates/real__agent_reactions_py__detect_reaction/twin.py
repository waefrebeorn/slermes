"""twin: detect_reaction(text) -> "vibe" or None. Input is HEX of the UTF-8 text."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/reactions.py"; PY_FN = "detect_reaction"
    C_FN = "agent_reactions_u_detect_reaction"
    def call(self, inp, inputs, idx=0):
        return self._fn(bytes.fromhex(str(inp)).decode("utf-8"))
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
