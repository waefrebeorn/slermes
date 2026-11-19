"""twin for hermes_cli/nous_subscription.py:stt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/nous_subscription.py"
    PY_FN = "stt"
    C_FN = "show_section_stt"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
