"""twin for gateway/run.py:_record_coalesced_completion_siblings (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_record_coalesced_completion_siblings"
    C_FN = "_record_coalesced_completion_siblings"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
