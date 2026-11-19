"""twin for tools/environments/local.py:_plugin_terminal_env_strip_keys (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/local.py"
    PY_FN = "_plugin_terminal_env_strip_keys"
    C_FN = "_plugin_terminal_env_strip_keys"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
