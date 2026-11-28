"""twin: agent/coding_context.py:_model_family, LIVE."""
import agent.coding_context as _cc
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/coding_context.py"
    PY_FN = "_model_family"
    C_FN = "_model_family"
    NORM_NONE = True
    def call(self, inp, inputs, idx=0):
        return _cc._model_family(inp)
    def norm(self, c, result):
        c.valstr("None" if result is None else result)
_t=_Twin(); trace=_t.trace; run=_t.run
