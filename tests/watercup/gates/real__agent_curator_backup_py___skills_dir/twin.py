"""twin for agent/curator_backup.py:_skills_dir (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/curator_backup.py"
    PY_FN = "_skills_dir"
    C_FN = "_skills_dir"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
