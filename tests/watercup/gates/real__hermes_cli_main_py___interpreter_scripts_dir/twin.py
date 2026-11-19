"""twin for hermes_cli/main.py:_interpreter_scripts_dir (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/main.py"
    PY_FN = "_interpreter_scripts_dir"
    C_FN = "_interpreter_scripts_dir"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
