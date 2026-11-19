"""twin for agent/onboarding.py:busy_input_hint_cli (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/onboarding.py"
    PY_FN = "busy_input_hint_cli"
    C_FN = "busy_input_hint_cli"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
