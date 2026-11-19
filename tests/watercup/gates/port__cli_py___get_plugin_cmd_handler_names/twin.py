"""twin for cli.py:_get_plugin_cmd_handler_names (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_get_plugin_cmd_handler_names"
    C_FN = "_get_plugin_cmd_handler_names"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
