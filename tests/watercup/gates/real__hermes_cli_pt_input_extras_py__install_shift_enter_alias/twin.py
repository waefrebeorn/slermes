"""twin for hermes_cli/pt_input_extras.py:install_shift_enter_alias (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/pt_input_extras.py"
    PY_FN = "install_shift_enter_alias"
    C_FN = "install_shift_enter_alias"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
