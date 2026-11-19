"""twin for hermes_cli/auth_commands.py:_normalize_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth_commands.py"
    PY_FN = "_normalize_provider"
    C_FN = "_normalize_provider"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
