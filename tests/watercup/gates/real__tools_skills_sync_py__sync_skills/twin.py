"""twin for tools/skills_sync.py:sync_skills (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_sync.py"
    PY_FN = "sync_skills"
    C_FN = "sync_skills"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
