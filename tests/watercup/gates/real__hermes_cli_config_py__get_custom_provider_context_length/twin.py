"""twin for hermes_cli/config.py:get_custom_provider_context_length (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/config.py"
    PY_FN = "get_custom_provider_context_length"
    C_FN = "cfg_get_custom_provider_context_length"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
