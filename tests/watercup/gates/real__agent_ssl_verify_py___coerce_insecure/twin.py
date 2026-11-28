"""twin: agent/ssl_verify.py:_coerce_insecure."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/ssl_verify.py"; PY_FN = "_coerce_insecure"
    C_FN = "agent_ssl_verify_coerce_insecure"
    def call(self, inp, inputs, idx=0): return self._fn(inp)
    def norm(self, c, result): c.valstr("True" if result else "False")
_t=_Twin(); trace=_t.trace; run=_t.run
