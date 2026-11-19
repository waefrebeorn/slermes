"""twin for hermes_cli/skills_hub.py:_prompt_for_skill_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/skills_hub.py"
    PY_FN = "_prompt_for_skill_name"
    C_FN = "hermes_cli_skills_hub_u_prompt_for_skill_name"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
