"""twin for hermes_cli/commands.py:is_interrupt_then_dispatch (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/commands.py"
    PY_FN = "is_interrupt_then_dispatch"
    C_FN = "cli_commands_is_interrupt_then_dispatch"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
