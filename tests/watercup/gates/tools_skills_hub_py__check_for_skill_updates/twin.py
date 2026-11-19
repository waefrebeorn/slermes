"""twin for tools/skills_hub.py:check_for_skill_updates (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_hub.py"
    PY_FN = "check_for_skill_updates"
    C_FN = "skills_hub_check_updates"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
