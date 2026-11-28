"""twin: _strip_reference_wrappers(text) over the raw input string."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/context_references.py"; PY_FN = "_strip_reference_wrappers"
    C_FN = "agent_context_references_u_strip_reference_wrappers"
    def call(self, inp, inputs, idx=0):
        return self._fn(inp)
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
