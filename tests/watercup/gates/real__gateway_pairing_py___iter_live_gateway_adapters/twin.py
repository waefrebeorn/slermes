"""twin for gateway/pairing.py:_iter_live_gateway_adapters (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/pairing.py"
    PY_FN = "_iter_live_gateway_adapters"
    C_FN = "_iter_live_gateway_adapters"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
