"""twin for agent/provider_projection.py:splice_provider_projection (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/provider_projection.py"
    PY_FN = "splice_provider_projection"
    C_FN = "splice_provider_projection"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
