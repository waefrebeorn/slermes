"""twin for hermes_cli/moa_config.py:coerce_privacy_filter (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/moa_config.py"
    PY_FN = "coerce_privacy_filter"
    C_FN = "hermes_cli_moa_config_coerce_privacy_filter"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
