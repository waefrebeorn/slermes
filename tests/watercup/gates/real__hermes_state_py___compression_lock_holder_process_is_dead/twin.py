"""twin for hermes_state.py:_compression_lock_holder_process_is_dead (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_compression_lock_holder_process_is_dead"
    C_FN = "hermes_state_lock_holder_process_is_dead"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
