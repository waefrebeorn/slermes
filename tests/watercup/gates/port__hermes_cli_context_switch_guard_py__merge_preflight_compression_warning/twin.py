"""twin for hermes_cli/context_switch_guard.py:merge_preflight_compression_warning (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/context_switch_guard.py"
    PY_FN = "merge_preflight_compression_warning"
    C_FN = "merge_preflight_compression_warning"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
