"""twin for agent/shared_metrics_contract.py:is_valid_execution_surface (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/shared_metrics_contract.py"
    PY_FN = "is_valid_execution_surface"
    C_FN = "smc_is_valid_execution_surface"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
