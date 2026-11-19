"""twin for hermes_cli/observability/shared_metrics_contract.py:client_resource_is_valid (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/observability/shared_metrics_contract.py"
    PY_FN = "client_resource_is_valid"
    C_FN = "smc_client_resource_is_valid"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
