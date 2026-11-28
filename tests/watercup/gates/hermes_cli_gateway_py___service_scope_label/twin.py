"""twin for hermes_cli/gateway.py:_service_scope_label(bool) -> "system"/"user"."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/gateway.py"
    PY_FN = "_service_scope_label"
    C_FN = "cgw_u_service_scope_label"
    def call(self, inp, inputs, idx=0):
        s = str(inp).strip(); low = s.lower()
        return self._fn(s == "1" or low == "true")
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
