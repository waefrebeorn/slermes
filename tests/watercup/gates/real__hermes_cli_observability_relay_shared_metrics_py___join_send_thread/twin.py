"""twin for hermes_cli/observability/relay_shared_metrics.py:_join_send_thread (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/observability/relay_shared_metrics.py"
    PY_FN = "_join_send_thread"
    C_FN = "_join_send_thread"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
