"""twin for agent/anthropic_credentials.py:_append_spent_rotation_sidecar (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_credentials.py"
    PY_FN = "_append_spent_rotation_sidecar"
    C_FN = "_append_spent_rotation_sidecar"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
