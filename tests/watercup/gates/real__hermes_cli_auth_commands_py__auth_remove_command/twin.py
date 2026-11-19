"""twin for hermes_cli/auth_commands.py:auth_remove_command (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth_commands.py"
    PY_FN = "auth_remove_command"
    C_FN = "auth_remove_command"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
