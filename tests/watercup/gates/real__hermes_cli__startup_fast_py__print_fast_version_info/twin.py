"""twin for hermes_cli/_startup_fast.py:print_fast_version_info (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_startup_fast.py"
    PY_FN = "print_fast_version_info"
    C_FN = "sf_print_fast_version_info"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
