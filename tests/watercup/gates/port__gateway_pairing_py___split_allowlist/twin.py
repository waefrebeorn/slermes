"""twin: gateway/pairing.py:_split_allowlist."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "gateway/pairing.py"; PY_FN = "_split_allowlist"
    C_FN = "pairing_split_allowlist_json"
    def call(self, inp, inputs, idx=0): return self._fn(inp)
    def norm(self, c, result): c.valstr(_j.dumps(list(result), separators=(",",":")))
_t=_Twin(); trace=_t.trace; run=_t.run
