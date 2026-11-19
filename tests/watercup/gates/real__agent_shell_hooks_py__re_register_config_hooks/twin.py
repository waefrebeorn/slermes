"""twin for agent/shell_hooks.py:re_register_config_hooks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/shell_hooks.py"
    PY_FN = "re_register_config_hooks"
    C_FN = "shell_hooks_re_register_config_hooks"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
