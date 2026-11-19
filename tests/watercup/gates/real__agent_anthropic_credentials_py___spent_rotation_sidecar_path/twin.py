"""twin for agent/anthropic_credentials.py:_spent_rotation_sidecar_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_credentials.py"
    PY_FN = "_spent_rotation_sidecar_path"
    C_FN = "_spent_rotation_sidecar_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
