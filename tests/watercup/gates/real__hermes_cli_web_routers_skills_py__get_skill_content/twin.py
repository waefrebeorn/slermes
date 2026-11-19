"""twin for hermes_cli/web_routers/skills.py:get_skill_content (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_routers/skills.py"
    PY_FN = "get_skill_content"
    C_FN = "get_skill_content"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
