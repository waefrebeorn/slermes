"""twin for hermes_cli/hooks.py:_cmd_list (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/hooks.py"
    PY_FN = "_cmd_list"
    C_FN = "cli_hermes_cli_hooks__cmd_list"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
