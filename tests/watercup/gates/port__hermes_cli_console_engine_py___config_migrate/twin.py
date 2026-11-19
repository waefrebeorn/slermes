"""twin for hermes_cli/console_engine.py:_config_migrate (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/console_engine.py"
    PY_FN = "_config_migrate"
    C_FN = "coe_config_migrate"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
