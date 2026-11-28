"""twin: POSIX shlex.quote is the reference semantics; result compared as hex."""
import shlex
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "hermes_constants_scratch_stuck.py"
    PY_FN = "shlex_quote"
    C_FN = "hcsstuck_shlex_quote"
    def call(self, inp, inputs, idx=0):
        if inp == "<<CAFE>>":
            inp = "caf\u00e9"
        return shlex.quote(inp)
    def norm(self, c, result):
        c.valstr(result.encode("utf-8").hex())
_t=_Twin(); trace=_t.trace; run=_t.run
