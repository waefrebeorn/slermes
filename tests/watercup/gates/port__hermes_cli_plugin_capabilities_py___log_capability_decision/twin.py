"""twin for hermes_cli/plugin_capabilities.py:_log_capability_decision (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugin_capabilities.py"
    PY_FN = "_log_capability_decision"
    C_FN = "_log_capability_decision"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
