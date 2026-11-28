"""twin for hermes_cli/session_export.py:_messages (session JSON -> list of dicts)."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_export.py"
    PY_FN = "_messages"
    C_FN = "sexp_u_messages"
    def call(self, inp, inputs, idx=0):
        return self._fn(_j.loads(inp))
    def norm(self, c, result):
        c.valstr(_j.dumps(list(result), separators=(",", ":")))
_t = _Twin(); trace = _t.trace; run = _t.run
