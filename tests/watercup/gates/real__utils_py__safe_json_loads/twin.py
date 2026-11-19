"""twin for utils.py:safe_json_loads (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "utils.py"
    PY_FN = "safe_json_loads"
    C_FN = "safe_json_loads"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
