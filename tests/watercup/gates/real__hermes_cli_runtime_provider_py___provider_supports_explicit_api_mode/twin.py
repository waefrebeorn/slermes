"""twin for hermes_cli/runtime_provider.py:_provider_supports_explicit_api_mode (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/runtime_provider.py"
    PY_FN = "_provider_supports_explicit_api_mode"
    C_FN = "provider_supports_explicit_api_mode"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
