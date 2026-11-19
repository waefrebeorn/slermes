"""twin for hermes_cli/tools_config.py:_cua_windows_install_lock_file (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/tools_config.py"
    PY_FN = "_cua_windows_install_lock_file"
    C_FN = "_cua_windows_install_lock_file"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
