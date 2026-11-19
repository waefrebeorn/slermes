"""twin for hermes_cli/update_cmd.py:_refuse_update_if_venv_foreign_owned (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_refuse_update_if_venv_foreign_owned"
    C_FN = "_refuse_update_if_venv_foreign_owned"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
