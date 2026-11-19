"""twin for tools/skills_sync.py:_find_installed_skill_dir_by_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_sync.py"
    PY_FN = "_find_installed_skill_dir_by_name"
    C_FN = "_find_installed_skill_dir_by_name"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
