"""twin for hermes_cli/curator.py:_print_unmanaged_summary (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/curator.py"
    PY_FN = "_print_unmanaged_summary"
    C_FN = "curator_print_unmanaged_summary"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
