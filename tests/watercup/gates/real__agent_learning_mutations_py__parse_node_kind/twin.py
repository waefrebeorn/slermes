"""twin: agent/learning_mutations.py:parse_node_kind."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/learning_mutations.py"; PY_FN = "parse_node_kind"
    C_FN = "learning_mutations_parse_node_kind"
    def call(self, inp, inputs, idx=0): return self._fn(inp)
    def norm(self, c, result): c.valstr(result)
_t=_Twin(); trace=_t.trace; run=_t.run
