"""twin for tools/skills_sync.py:list_user_modified_bundled_skills (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_sync.py"
    PY_FN = "list_user_modified_bundled_skills"
    C_FN = "list_user_modified_bundled_skills"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
