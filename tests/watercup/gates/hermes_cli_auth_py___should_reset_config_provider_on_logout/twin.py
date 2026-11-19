"""twin for hermes_cli/auth.py:_should_reset_config_provider_on_logout (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/auth.py"
    PY_FN = "_should_reset_config_provider_on_logout"
    C_FN = "auth_should_reset_config_provider_on_logout"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
