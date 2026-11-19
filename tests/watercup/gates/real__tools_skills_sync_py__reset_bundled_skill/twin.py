"""twin for tools/skills_sync.py:reset_bundled_skill (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_sync.py"
    PY_FN = "reset_bundled_skill"
    C_FN = "reset_bundled_skill"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
