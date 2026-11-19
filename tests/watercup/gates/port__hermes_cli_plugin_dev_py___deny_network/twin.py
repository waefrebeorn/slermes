"""twin for hermes_cli/plugin_dev.py:_deny_network (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugin_dev.py"
    PY_FN = "_deny_network"
    C_FN = "hermes_cli_plugin_dev_deny_network"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
