"""twin for hermes_state.py:restore_compression_failure_cooldown_row (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "restore_compression_failure_cooldown_row"
    C_FN = "restore_compression_failure_cooldown_row"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
