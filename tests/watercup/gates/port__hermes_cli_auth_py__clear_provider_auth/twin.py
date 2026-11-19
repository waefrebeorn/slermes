"""twin for hermes_cli/auth.py:clear_provider_auth (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth.py"
    PY_FN = "clear_provider_auth"
    C_FN = "clear_provider_auth"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
