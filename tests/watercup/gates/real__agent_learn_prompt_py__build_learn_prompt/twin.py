"""twin for agent/learn_prompt.py:build_learn_prompt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/learn_prompt.py"
    PY_FN = "build_learn_prompt"
    C_FN = "build_learn_prompt"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
