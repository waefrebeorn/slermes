"""twin for tools/bot_failure_reasons.py:classify_agent_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/bot_failure_reasons.py"
    PY_FN = "classify_agent_error"
    C_FN = "classify_agent_error"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
