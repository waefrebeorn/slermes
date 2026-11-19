"""twin for hermes_cli/config.py:_custom_provider_entry_to_provider_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/config.py"
    PY_FN = "_custom_provider_entry_to_provider_config"
    C_FN = "_custom_provider_entry_to_provider_config"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
