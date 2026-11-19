"""twin for hermes_cli/subcommands/pause.py:cmd_resume (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/subcommands/pause.py"
    PY_FN = "cmd_resume"
    C_FN = "cmd_resume"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
