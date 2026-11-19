"""twin for hermes_cli/observability/relay_shared_metrics.py:_turn_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/observability/relay_shared_metrics.py"
    PY_FN = "_turn_key"
    C_FN = "_turn_key"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
