"""twin for hermes_cli/tools_config.py:_shared_metrics_menu_label (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/tools_config.py"
    PY_FN = "_shared_metrics_menu_label"
    C_FN = "_shared_metrics_menu_label"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
