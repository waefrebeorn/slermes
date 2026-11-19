"""twin for hermes_cli/send_cmd.py:register_send_subparser (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/send_cmd.py"
    PY_FN = "register_send_subparser"
    C_FN = "register_send_subparser"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
