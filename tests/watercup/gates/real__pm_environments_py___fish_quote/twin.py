"""twin: pm/environments.py:_fish_quote. Compared as hex (see harness)."""
import pm.environments as _env
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "pm/environments.py"
    PY_FN = "_fish_quote"
    C_FN = "w5_environments_407599_fish_quote"
    def call(self, inp, inputs, idx=0):
        return _env._fish_quote(inp)
    def norm(self, c, result):
        c.valstr(result.encode("utf-8").hex())
_t=_Twin(); trace=_t.trace; run=_t.run
