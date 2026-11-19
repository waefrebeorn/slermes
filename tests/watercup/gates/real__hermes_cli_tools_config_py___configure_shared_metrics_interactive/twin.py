"""twin for hermes_cli/tools_config.py:_configure_shared_metrics_interactive (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/tools_config.py"
    PY_FN = "_configure_shared_metrics_interactive"
    C_FN = "_configure_shared_metrics_interactive"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
