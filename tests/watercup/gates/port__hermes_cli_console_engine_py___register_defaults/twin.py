"""twin for hermes_cli/console_engine.py:_register_defaults (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/console_engine.py"
    PY_FN = "_register_defaults"
    C_FN = "coe_register_defaults"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
