"""twin for tools/tirith_security.py:check_command_security (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tirith_security.py"
    PY_FN = "check_command_security"
    C_FN = "check_command_security"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
