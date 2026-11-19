"""twin for agent/file_safety.py:_resolve_active_profile_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/file_safety.py"
    PY_FN = "_resolve_active_profile_name"
    C_FN = "_resolve_active_profile_name"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
