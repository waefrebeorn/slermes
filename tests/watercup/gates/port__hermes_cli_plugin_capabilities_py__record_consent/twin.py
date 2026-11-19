"""twin for hermes_cli/plugin_capabilities.py:record_consent (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugin_capabilities.py"
    PY_FN = "record_consent"
    C_FN = "record_consent"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
