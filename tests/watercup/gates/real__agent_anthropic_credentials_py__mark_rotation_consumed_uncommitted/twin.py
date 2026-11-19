"""twin for agent/anthropic_credentials.py:mark_rotation_consumed_uncommitted (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_credentials.py"
    PY_FN = "mark_rotation_consumed_uncommitted"
    C_FN = "mark_rotation_consumed_uncommitted"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
