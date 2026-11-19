"""twin for hermes_cli/plugins_cmd.py:_clear_plugin_bytecode (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins_cmd.py"
    PY_FN = "_clear_plugin_bytecode"
    C_FN = "pcmd_remove_pycache_in_dir"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
