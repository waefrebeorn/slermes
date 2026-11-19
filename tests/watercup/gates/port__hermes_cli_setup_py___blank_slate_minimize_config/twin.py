"""twin for hermes_cli/setup.py:_blank_slate_minimize_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/setup.py"
    PY_FN = "_blank_slate_minimize_config"
    C_FN = "_blank_slate_minimize_config"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
