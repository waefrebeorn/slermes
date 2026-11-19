"""twin for tools/bot_failure_reasons.py:retry_action (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/bot_failure_reasons.py"
    PY_FN = "retry_action"
    C_FN = "retry_action"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
