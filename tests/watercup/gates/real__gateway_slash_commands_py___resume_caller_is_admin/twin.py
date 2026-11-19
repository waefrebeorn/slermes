"""twin for gateway/slash_commands.py:_resume_caller_is_admin (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/slash_commands.py"
    PY_FN = "_resume_caller_is_admin"
    C_FN = "_resume_caller_is_admin"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
