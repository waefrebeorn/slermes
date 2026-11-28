"""twin for agent/turn_summary.py _pluralize(count, plural_noun). Input "count\tnoun"."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/turn_summary.py"; PY_FN = "_pluralize"
    C_FN = "agent_turn_summary_u_pluralize"
    def call(self, inp, inputs, idx=0):
        s = str(inp); cnt, _, noun = s.partition("\t")
        return self._fn(int(cnt), noun)
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
