"""twin for hermes_cli/tools_config.py:_cua_install_target_writable (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/tools_config.py"
    PY_FN = "_cua_install_target_writable"
    C_FN = "tools_config_cua_install_target_writable"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
