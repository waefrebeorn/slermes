"""twin for hermes_cli/plugins.py:_reset_plugin_managers_for_tests (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "_reset_plugin_managers_for_tests"
    C_FN = "_reset_plugin_managers_for_tests"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
