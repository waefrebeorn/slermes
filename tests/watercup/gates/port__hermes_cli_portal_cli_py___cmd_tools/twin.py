"""twin for hermes_cli/portal_cli.py:_cmd_tools (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/portal_cli.py"
    PY_FN = "_cmd_tools"
    C_FN = "cmd_tools"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
