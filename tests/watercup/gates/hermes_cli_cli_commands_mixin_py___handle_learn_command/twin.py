"""twin for hermes_cli/cli_commands_mixin.py:_handle_learn_command (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/cli_commands_mixin.py"
    PY_FN = "_handle_learn_command"
    C_FN = "cmd_learn"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
