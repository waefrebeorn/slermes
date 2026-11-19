"""twin for hermes_cli/auth.py:is_provider_explicitly_configured (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth.py"
    PY_FN = "is_provider_explicitly_configured"
    C_FN = "is_provider_explicitly_configured"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
