"""twin for hermes_cli/update_cmd.py:_venv_launcher_ancestors (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_venv_launcher_ancestors"
    C_FN = "_venv_launcher_ancestors"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
