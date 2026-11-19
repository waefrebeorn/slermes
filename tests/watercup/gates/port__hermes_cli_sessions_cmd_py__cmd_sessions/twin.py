"""twin for hermes_cli/sessions_cmd.py:cmd_sessions (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/sessions_cmd.py"
    PY_FN = "cmd_sessions"
    C_FN = "cmd_sessions"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
