"""twin for hermes_cli/banner.py:prefetch_update_check (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/banner.py"
    PY_FN = "prefetch_update_check"
    C_FN = "banner_prefetch_update_check"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
