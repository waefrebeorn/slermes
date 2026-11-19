"""twin for tools/skills_sync.py:_is_tracked_user_modification (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_sync.py"
    PY_FN = "_is_tracked_user_modification"
    C_FN = "sys_is_tracked_user_modification"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
