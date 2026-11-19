"""twin for hermes_cli/slash_exec.py:execute_command (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/slash_exec.py"
    PY_FN = "execute_command"
    C_FN = "slash_exec_execute"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
