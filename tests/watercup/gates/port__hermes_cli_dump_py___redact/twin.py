"""twin: hermes_cli/dump.py:_redact -> agent.redact.mask_secret defaults."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dump.py"; PY_FN = "_redact"
    C_FN = "red_mask_secret"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp))
    def norm(self, c, result):
        c.valstr(result if result is not None else "")
_t=_Twin(); trace=_t.trace; run=_t.run
