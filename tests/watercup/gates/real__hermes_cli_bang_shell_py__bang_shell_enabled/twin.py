"""twin for hermes_cli/bang_shell.py:bang_shell_enabled (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/bang_shell.py"
    PY_FN = "bang_shell_enabled"
    C_FN = "bang_shell_enabled"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
