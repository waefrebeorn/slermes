"""twin for agent/shell_hooks.py:register_from_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/shell_hooks.py"
    PY_FN = "register_from_config"
    C_FN = "shk_register_from_config"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
