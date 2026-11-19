"""twin for hermes_cli/observability/shared_metrics_send_config.py:_endpoint_is_safe (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/observability/shared_metrics_send_config.py"
    PY_FN = "_endpoint_is_safe"
    C_FN = "_endpoint_is_safe"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
