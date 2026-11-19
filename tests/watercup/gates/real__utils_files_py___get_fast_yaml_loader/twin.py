"""twin for utils.py:_get_fast_yaml_loader (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "utils.py"
    PY_FN = "_get_fast_yaml_loader"
    C_FN = "_get_fast_yaml_loader"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
