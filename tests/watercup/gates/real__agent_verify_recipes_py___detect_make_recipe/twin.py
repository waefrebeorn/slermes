"""twin for agent/verify/recipes.py:_detect_make_recipe (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/verify/recipes.py"
    PY_FN = "_detect_make_recipe"
    C_FN = "_detect_make_recipe"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
