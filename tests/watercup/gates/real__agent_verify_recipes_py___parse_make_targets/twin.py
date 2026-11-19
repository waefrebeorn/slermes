"""twin for agent/verify/recipes.py:_parse_make_targets (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/verify/recipes.py"
    PY_FN = "_parse_make_targets"
    C_FN = "_parse_make_targets"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
