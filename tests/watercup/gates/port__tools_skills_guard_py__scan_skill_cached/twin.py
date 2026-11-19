"""twin for tools/skills_guard.py:scan_skill_cached (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_guard.py"
    PY_FN = "scan_skill_cached"
    C_FN = "scan_skill_cached"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
