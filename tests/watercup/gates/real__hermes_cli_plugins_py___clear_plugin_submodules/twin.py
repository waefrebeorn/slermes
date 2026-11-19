"""twin for hermes_cli/plugins.py:_clear_plugin_submodules (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "_clear_plugin_submodules"
    C_FN = "_clear_plugin_submodules"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
