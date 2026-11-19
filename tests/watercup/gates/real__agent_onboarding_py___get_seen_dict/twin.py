"""twin for agent/onboarding.py:_get_seen_dict (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/onboarding.py"
    PY_FN = "_get_seen_dict"
    C_FN = "_get_seen_dict"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
