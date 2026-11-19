"""twin for hermes_cli/config.py:cfg_get (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/config.py"
    PY_FN = "cfg_get"
    C_FN = "cfg_get"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
