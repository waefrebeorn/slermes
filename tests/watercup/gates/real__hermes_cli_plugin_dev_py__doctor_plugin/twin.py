"""twin for hermes_cli/plugin_dev.py:doctor_plugin (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugin_dev.py"
    PY_FN = "doctor_plugin"
    C_FN = "doctor_plugin"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
