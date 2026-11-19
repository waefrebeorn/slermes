"""twin for hermes_cli/moa_cmd.py:cmd_moa (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/moa_cmd.py"
    PY_FN = "cmd_moa"
    C_FN = "cmd_moa"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
