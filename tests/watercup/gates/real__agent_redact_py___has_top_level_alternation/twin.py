"""twin: _has_top_level_alternation(pattern) -> bool."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/redact.py"; PY_FN = "_has_top_level_alternation"
    C_FN = "agent_redact_pattern_shape_u_has_top_level_alternation"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp))
    def norm(self, c, result):
        c.valstr("1" if result else "0")
_t = _Twin(); trace = _t.trace; run = _t.run
