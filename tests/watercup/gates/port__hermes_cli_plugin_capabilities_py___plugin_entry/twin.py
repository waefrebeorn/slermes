"""twin for hermes_cli/plugin_capabilities.py:_plugin_entry (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugin_capabilities.py"
    PY_FN = "_plugin_entry"
    C_FN = "_plugin_entry"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
