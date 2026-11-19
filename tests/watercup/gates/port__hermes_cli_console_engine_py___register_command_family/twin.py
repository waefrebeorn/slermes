"""twin for hermes_cli/console_engine.py:_register_command_family (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/console_engine.py"
    PY_FN = "_register_command_family"
    C_FN = "_register_command_family"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
