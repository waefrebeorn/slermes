"""twin for tools/skills_sync.py:_build_external_skill_index (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_sync.py"
    PY_FN = "_build_external_skill_index"
    C_FN = "_build_external_skill_index"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
