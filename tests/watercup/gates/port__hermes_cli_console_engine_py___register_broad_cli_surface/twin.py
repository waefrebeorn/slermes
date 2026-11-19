"""twin for hermes_cli/console_engine.py:_register_broad_cli_surface (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/console_engine.py"
    PY_FN = "_register_broad_cli_surface"
    C_FN = "coe_register_broad_cli_surface"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
