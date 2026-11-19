"""twin for hermes_state.py:_drain_token_queue_at_exit (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_drain_token_queue_at_exit"
    C_FN = "_drain_token_queue_at_exit"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
