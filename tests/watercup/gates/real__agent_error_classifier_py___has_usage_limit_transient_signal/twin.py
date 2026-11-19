"""twin for agent/error_classifier.py:_has_usage_limit_transient_signal (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/error_classifier.py"
    PY_FN = "_has_usage_limit_transient_signal"
    C_FN = "_has_usage_limit_transient_signal"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
