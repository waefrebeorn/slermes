"""twin for hermes_cli/context_switch_guard.py:enrich_model_switch_warnings_for_gateway (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/context_switch_guard.py"
    PY_FN = "enrich_model_switch_warnings_for_gateway"
    C_FN = "enrich_model_switch_warnings_for_gateway"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
