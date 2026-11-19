"""twin for hermes_cli/update_cmd.py:_detect_venv_python_processes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_detect_venv_python_processes"
    C_FN = "_detect_venv_python_processes"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
