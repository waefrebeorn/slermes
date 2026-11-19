"""twin for gateway/dead_targets.py:_load (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/dead_targets.py"
    PY_FN = "_load"
    C_FN = "dead_target_registry_load"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
