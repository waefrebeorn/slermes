"""twin for hermes_cli/plugins.py:_install_plugin_debug_handler (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "_install_plugin_debug_handler"
    C_FN = "plug_install_debug_handler"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
