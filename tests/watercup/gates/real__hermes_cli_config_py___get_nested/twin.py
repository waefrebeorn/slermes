"""twin for hermes_cli/config.py:_get_nested (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/config.py"
    PY_FN = "_get_nested"
    C_FN = "_get_nested"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
