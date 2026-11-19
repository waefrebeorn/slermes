"""twin for hermes_cli/cli_commands_mixin.py:_print_lightpanda_engine_status (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/cli_commands_mixin.py"
    PY_FN = "_print_lightpanda_engine_status"
    C_FN = "_print_lightpanda_engine_status"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
