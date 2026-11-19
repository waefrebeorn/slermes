"""twin for hermes_cli/setup.py:_supports_same_provider_pool_setup (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/setup.py"
    PY_FN = "_supports_same_provider_pool_setup"
    C_FN = "stp_supports_same_provider_pool_setup"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
