"""twin for gateway/run.py:_format_coalesced_async_delegations (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_format_coalesced_async_delegations"
    C_FN = "_format_coalesced_async_delegations"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
