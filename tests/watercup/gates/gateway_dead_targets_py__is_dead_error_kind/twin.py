"""twin for gateway/dead_targets.py:is_dead_error_kind (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/dead_targets.py"
    PY_FN = "is_dead_error_kind"
    C_FN = "dead_target_is_dead_error_kind"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
