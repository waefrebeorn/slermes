"""twin for gateway/run.py:_wrap_current_message_with_observed_context (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_wrap_current_message_with_observed_context"
    C_FN = "_wrap_current_message_with_observed_context"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
