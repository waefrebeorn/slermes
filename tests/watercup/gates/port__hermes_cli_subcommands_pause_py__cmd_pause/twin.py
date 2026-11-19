"""twin for hermes_cli/subcommands/pause.py:cmd_pause (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/subcommands/pause.py"
    PY_FN = "cmd_pause"
    C_FN = "cmd_pause"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
