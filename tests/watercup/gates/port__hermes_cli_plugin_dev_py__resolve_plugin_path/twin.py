"""twin for hermes_cli/plugin_dev.py:resolve_plugin_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugin_dev.py"
    PY_FN = "resolve_plugin_path"
    C_FN = "plugin_dev_resolve_plugin_path"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
