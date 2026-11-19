"""twin for hermes_cli/update_cmd.py:_called_process_error_is_python_dep_install (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_called_process_error_is_python_dep_install"
    C_FN = "upd_called_process_error_is_python_dep_install"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
