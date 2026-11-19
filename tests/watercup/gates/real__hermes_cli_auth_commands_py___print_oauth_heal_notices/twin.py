"""twin for hermes_cli/auth_commands.py:_print_oauth_heal_notices (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth_commands.py"
    PY_FN = "_print_oauth_heal_notices"
    C_FN = "_print_oauth_heal_notices"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
