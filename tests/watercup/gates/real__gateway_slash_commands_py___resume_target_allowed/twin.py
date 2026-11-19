"""twin for gateway/slash_commands.py:_resume_target_allowed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/slash_commands.py"
    PY_FN = "_resume_target_allowed"
    C_FN = "slash_resume_target_allowed"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
