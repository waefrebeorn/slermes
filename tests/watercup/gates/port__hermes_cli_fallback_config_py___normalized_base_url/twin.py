"""twin for hermes_cli/fallback_config.py:_normalized_base_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/fallback_config.py"
    PY_FN = "_normalized_base_url"
    C_FN = "_normalized_base_url"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
