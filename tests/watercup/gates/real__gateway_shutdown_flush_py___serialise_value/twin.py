"""twin for gateway/shutdown_flush.py:_serialise_value (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/shutdown_flush.py"
    PY_FN = "_serialise_value"
    C_FN = "_serialise_value"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
