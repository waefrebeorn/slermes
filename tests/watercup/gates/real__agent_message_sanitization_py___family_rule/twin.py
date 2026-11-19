"""twin for agent/message_sanitization.py:_family_rule (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/message_sanitization.py"
    PY_FN = "_family_rule"
    C_FN = "_family_rule"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
