"""twin for tools/skills_sync.py:is_bundled_skills_opt_out (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_sync.py"
    PY_FN = "is_bundled_skills_opt_out"
    C_FN = "is_bundled_skills_opt_out"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
