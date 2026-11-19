"""twin for agent/plan_prompt.py:build_plan_prompt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/plan_prompt.py"
    PY_FN = "build_plan_prompt"
    C_FN = "build_plan_prompt"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
