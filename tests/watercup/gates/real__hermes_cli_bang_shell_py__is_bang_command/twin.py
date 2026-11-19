"""twin for hermes_cli/bang_shell.py:is_bang_command (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/bang_shell.py"
    PY_FN = "is_bang_command"
    C_FN = "bang_shell_is_bang_command"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
