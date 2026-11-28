"""twin: codex_supported_efforts(model) -> tuple[str,...]."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/reasoning_effort.py"; PY_FN = "codex_supported_efforts"
    C_FN = "codex_supported_efforts"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp))
    def norm(self, c, result):
        c.valstr(_j.dumps(list(result), separators=(",", ":")))
_t=_Twin(); trace=_t.trace; run=_t.run
