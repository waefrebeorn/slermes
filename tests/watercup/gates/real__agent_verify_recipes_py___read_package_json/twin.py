"""twin for agent/verify/recipes.py:_read_package_json (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/verify/recipes.py"
    PY_FN = "_read_package_json"
    C_FN = "_read_package_json"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
