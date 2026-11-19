"""twin for hermes_cli/pt_input_extras.py:_lock_twins (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/pt_input_extras.py"
    PY_FN = "_lock_twins"
    C_FN = "_lock_twins"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
