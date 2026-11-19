"""twin for hermes_cli/plugins.py:get_plugin_toolset_keys_nowait (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "get_plugin_toolset_keys_nowait"
    C_FN = "get_plugin_toolset_keys_nowait"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
