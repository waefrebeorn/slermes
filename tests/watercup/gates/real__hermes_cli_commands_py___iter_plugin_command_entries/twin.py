"""twin for hermes_cli/commands.py:_iter_plugin_command_entries (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/commands.py"
    PY_FN = "_iter_plugin_command_entries"
    C_FN = "_iter_plugin_command_entries"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
