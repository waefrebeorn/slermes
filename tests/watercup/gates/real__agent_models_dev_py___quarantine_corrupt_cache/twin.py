"""twin for agent/models_dev.py:_quarantine_corrupt_cache (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/models_dev.py"
    PY_FN = "_quarantine_corrupt_cache"
    C_FN = "mdv_quarantine_corrupt_cache"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
