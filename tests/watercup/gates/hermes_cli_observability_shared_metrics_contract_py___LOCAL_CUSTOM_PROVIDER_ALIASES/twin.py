"""twin for hermes_cli/observability/shared_metrics_contract.py:_LOCAL_CUSTOM_PROVIDER_ALIASES (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/observability/shared_metrics_contract.py"
    PY_FN = "_LOCAL_CUSTOM_PROVIDER_ALIASES"
    C_FN = "smc_is_local_custom_alias"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
