"""twin for hermes_cli/plugins.py:_re_register_config_hooks_after_force (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugins.py"
    PY_FN = "_re_register_config_hooks_after_force"
    C_FN = "_re_register_config_hooks_after_force"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
