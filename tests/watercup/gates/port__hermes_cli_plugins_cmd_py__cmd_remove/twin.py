"""twin for hermes_cli/plugins_cmd.py:cmd_remove (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins_cmd.py"
    PY_FN = "cmd_remove"
    C_FN = "cmd_remove"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
