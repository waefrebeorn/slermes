"""twin for hermes_cli/observability/shared_metrics_contract.py:client_os_family (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/observability/shared_metrics_contract.py"
    PY_FN = "client_os_family"
    C_FN = "smc_client_os_family"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
