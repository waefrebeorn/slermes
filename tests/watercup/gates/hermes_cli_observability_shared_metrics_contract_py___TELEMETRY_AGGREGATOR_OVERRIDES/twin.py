"""twin for hermes_cli/observability/shared_metrics_contract.py:_TELEMETRY_AGGREGATOR_OVERRIDES (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/observability/shared_metrics_contract.py"
    PY_FN = "_TELEMETRY_AGGREGATOR_OVERRIDES"
    C_FN = "smc_is_aggregator_override"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
