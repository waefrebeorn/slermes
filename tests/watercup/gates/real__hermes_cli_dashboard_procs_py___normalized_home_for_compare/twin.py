"""twin for hermes_cli/dashboard_procs.py:_normalized_home_for_compare (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_procs.py"
    PY_FN = "_normalized_home_for_compare"
    C_FN = "_normalized_home_for_compare"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
