"""twin for hermes_cli/tools_config.py:_cua_driver_autostart_registered_windows (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/tools_config.py"
    PY_FN = "_cua_driver_autostart_registered_windows"
    C_FN = "tools_config_cua_driver_autostart_registered_windows"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
