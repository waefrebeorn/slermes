"""twin for hermes_cli/auth_commands.py:_configured_provider_entry (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth_commands.py"
    PY_FN = "_configured_provider_entry"
    C_FN = "_configured_provider_entry"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
