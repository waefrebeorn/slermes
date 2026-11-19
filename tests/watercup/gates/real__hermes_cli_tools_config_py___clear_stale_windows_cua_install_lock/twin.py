"""twin for hermes_cli/tools_config.py:_clear_stale_windows_cua_install_lock (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/tools_config.py"
    PY_FN = "_clear_stale_windows_cua_install_lock"
    C_FN = "tools_config_clear_stale_windows_cua_install_lock"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
