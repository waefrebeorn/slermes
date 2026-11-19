"""twin for hermes_cli/checkpoints.py:cmd_clear (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/checkpoints.py"
    PY_FN = "cmd_clear"
    C_FN = "cmd_clear"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
