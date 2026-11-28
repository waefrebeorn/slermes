"""twin for hermes_cli/session_export_md.py:_message_count (session JSON)."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_export_md.py"
    PY_FN = "_message_count"
    C_FN = "sexmd_u_message_count"
    def call(self, inp, inputs, idx=0):
        return self._fn(_j.loads(inp))
    def norm(self, c, result):
        c.valstr(str(int(result)))
_t = _Twin(); trace = _t.trace; run = _t.run
