"""twin for hermes_cli/proxy_cli.py:cmd_restart (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/proxy_cli.py"
    PY_FN = "cmd_restart"
    C_FN = "cmd_restart"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
