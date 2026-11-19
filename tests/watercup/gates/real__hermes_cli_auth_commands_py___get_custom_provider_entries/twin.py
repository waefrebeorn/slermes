"""twin for hermes_cli/auth_commands.py:_get_custom_provider_entries (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth_commands.py"
    PY_FN = "_get_custom_provider_entries"
    C_FN = "_get_custom_provider_entries"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
