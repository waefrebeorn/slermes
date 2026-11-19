"""twin for hermes_cli/_install_repair.py:_load_console_script_names (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_install_repair.py"
    PY_FN = "_load_console_script_names"
    C_FN = "_load_console_script_names"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
