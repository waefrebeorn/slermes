"""twin for hermes_cli/observability/shared_metrics_sender.py:_isoformat (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/observability/shared_metrics_sender.py"
    PY_FN = "_isoformat"
    C_FN = "_isoformat"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
