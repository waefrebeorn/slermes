"""twin: is_dead_error_kind -- python exposes no function; the authority is the
_DEAD_ERROR_KINDS frozenset, so membership is the contract being gated."""
from watercup_twin_runtime import TwinBase
from gateway.dead_targets import _DEAD_ERROR_KINDS
class _Twin(TwinBase):
    PY_MODULE = "gateway/dead_targets.py"; PY_FN = "<set>_DEAD_ERROR_KINDS"
    C_FN = "dead_target_is_dead_error_kind"
    def call(self, inp, inputs, idx=0): return inp in _DEAD_ERROR_KINDS
    def norm(self, c, result): c.valstr("True" if result else "False")
_t=_Twin(); trace=_t.trace; run=_t.run
