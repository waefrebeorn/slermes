"""twin for tools/skills_guard.py:_shell_write_re (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_guard.py"
    PY_FN = "_shell_write_re"
    C_FN = "_shell_write_re"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
