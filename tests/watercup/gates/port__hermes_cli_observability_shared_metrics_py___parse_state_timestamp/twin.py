"""twin for hermes_cli/observability/shared_metrics.py:_parse_state_timestamp (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/observability/shared_metrics.py"
    PY_FN = "_parse_state_timestamp"
    C_FN = "_parse_state_timestamp"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
