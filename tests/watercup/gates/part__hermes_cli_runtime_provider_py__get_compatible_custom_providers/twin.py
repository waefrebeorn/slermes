"""twin for hermes_cli/runtime_provider.py:get_compatible_custom_providers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/runtime_provider.py"
    PY_FN = "get_compatible_custom_providers"
    C_FN = "get_compatible_custom_providers"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
