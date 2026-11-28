"""twin: _classify_op_error -> ErrorKind.value."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/onepassword.py"; PY_FN = "_classify_op_error"
    C_FN = "agent_secret_sources_onepassword_u_classify_op_error"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp))
    def norm(self, c, result):
        c.valstr(getattr(result, "value", str(result)))
_t = _Twin(); trace = _t.trace; run = _t.run
