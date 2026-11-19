"""twin for hermes_cli/bang_shell.py:check_bang_approval (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/bang_shell.py"
    PY_FN = "check_bang_approval"
    C_FN = "check_bang_approval"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
