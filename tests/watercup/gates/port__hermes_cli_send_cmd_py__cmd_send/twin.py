"""twin for hermes_cli/send_cmd.py:cmd_send (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/send_cmd.py"
    PY_FN = "cmd_send"
    C_FN = "cmd_send"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
