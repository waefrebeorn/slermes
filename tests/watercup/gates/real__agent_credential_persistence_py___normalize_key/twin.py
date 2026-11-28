"""twin: _normalize_key (@@ARGS@@)."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/credential_persistence.py"; PY_FN = "_normalize_key"; C_FN = "@@CF@@"
    def call(self, inp, inputs, idx=0):
        return self._fn(inp)
    def norm(self, c, result):
        c.valstr("" if result is None else (("True" if result else "False") if isinstance(result,bool) else result))
_t=_Twin(); trace=_t.trace; run=_t.run
