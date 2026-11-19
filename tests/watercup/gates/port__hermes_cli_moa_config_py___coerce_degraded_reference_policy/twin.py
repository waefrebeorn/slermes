"""twin for hermes_cli/moa_config.py:_coerce_degraded_reference_policy (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/moa_config.py"
    PY_FN = "_coerce_degraded_reference_policy"
    C_FN = "hermes_cli_moa_config_u_coerce_degraded_reference_policy"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
